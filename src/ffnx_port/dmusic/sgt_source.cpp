// sgt_source.cpp - FF8's 2013 music on Android: FFNx plays FF8's music by name through its own
// audio engine (use_external_music, music.cpp); here a name finds the game's own file and a
// DirectMusic segment becomes a SoLoud source (ff8_dmusic.cpp plays it with FF8.dls).
//
// Names: FFNx asks for "battle" (the segment 005s-battle.sgt without its number and type), or a
// streamed track's name (Data/Music/stream/*.wav, played by vgmstream as before). Where two
// segments share a name, the software-synth one ("s") is taken over the XG one ("xg").
#include <dirent.h>
#include <strings.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <soloud.h>
#include "ff8_dmusic.h"

extern char basedir[];                    // FFNx: the game folder

namespace {

std::string lower(std::string s) {
    for (char &c : s) c = char(std::tolower((unsigned char)c));
    return s;
}

/** The game folder's directory, matched ignoring case (Data/Music/dmusic on a case-sensitive disk). */
std::string find_dir(const std::string &root, const char *const *parts, int n) {
    std::string at = root;
    for (int i = 0; i < n; i++) {
        DIR *d = opendir(at.c_str());
        if (!d) return "";
        std::string hit;
        while (dirent *e = readdir(d)) if (!strcasecmp(e->d_name, parts[i])) { hit = e->d_name; break; }
        closedir(d);
        if (hit.empty()) return "";
        at += "/" + hit;
    }
    return at;
}

struct Index {
    std::map<std::string, std::pair<int, std::string>> by_name;   // name -> (priority, path)
    std::string dls;
};

void add(Index &ix, const std::string &key, int prio, const std::string &path) {
    auto it = ix.by_name.find(key);
    if (it == ix.by_name.end() || prio > it->second.first) ix.by_name[key] = {prio, path};
}

void scan(Index &ix, const std::string &dir, const char *ext, bool segments) {
    if (dir.empty()) return;
    DIR *d = opendir(dir.c_str());
    if (!d) return;
    while (dirent *e = readdir(d)) {
        std::string n = e->d_name, l = lower(n);
        size_t dot = l.rfind('.');
        if (dot == std::string::npos || l.substr(dot) != ext) {
            if (segments && l == "ff8.dls") ix.dls = dir + "/" + n;
            continue;
        }
        std::string stem = l.substr(0, dot), path = dir + "/" + n;
        size_t dash = stem.find('-');
        int prio = segments ? 2 : 1;
        if (dash != std::string::npos) {
            std::string type = stem.substr(0, dash);
            if (type.find("xg") != std::string::npos) prio -= 1;   // XG variant: the software one wins
            add(ix, stem.substr(dash + 1), prio, path);
        }
        add(ix, stem, prio + 2, path);                              // the full name too
    }
    closedir(d);
}

std::mutex g_mutex;
std::unique_ptr<Index> g_index;
std::shared_ptr<const dmusic::Dls> g_dls;

Index &index() {
    if (!g_index) {
        g_index.reset(new Index);
        static const char *const dm[] = { "data", "music", "dmusic" }, *const st[] = { "data", "music", "stream" }, *const mu[] = { "data", "music" };
        std::string root = basedir;
        scan(*g_index, find_dir(root, dm, 3), ".sgt", true);
        scan(*g_index, find_dir(root, st, 3), ".wav", false);
        scan(*g_index, find_dir(root, mu, 2), ".wav", false);
        std::fprintf(stderr, "[ff8-music] %zu tracks, DLS %s\n", g_index->by_name.size(), g_index->dls.empty() ? "missing" : "found");
    }
    return *g_index;
}

}  // namespace

/** FFNx NxAudioEngine::getFilenameFullPath (music): the game's own file for a music name. */
extern "C" bool ffnx_ff8_music_file(char *out, size_t out_size, const char *key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = index().by_name.find(lower(key));
    if (it == index().by_name.end()) return false;
    std::snprintf(out, out_size, "%s", it->second.second.c_str());
    return true;
}

namespace SoLoud {

class DmusicSegmentInstance;

class DmusicSegment : public AudioSource {
public:
    std::shared_ptr<const dmusic::Dls> mDls;
    std::shared_ptr<const dmusic::Segment> mSeg;
    double mLength = 0;
    static constexpr int RATE = 44100;

    result load(const char *path) {
        std::string err;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!g_dls) {
                const std::string &d = index().dls;
                if (d.empty()) { std::fprintf(stderr, "[ff8-music] no FF8.dls next to the segments\n"); return FILE_NOT_FOUND; }
                g_dls = dmusic::load_dls(d, &err);
                if (!g_dls) { std::fprintf(stderr, "[ff8-music] %s: %s\n", d.c_str(), err.c_str()); return FILE_LOAD_FAILED; }
            }
            mDls = g_dls;
        }
        mSeg = dmusic::load_segment(path, &err);
        if (!mSeg) { std::fprintf(stderr, "[ff8-music] %s: %s\n", path, err.c_str()); return FILE_LOAD_FAILED; }
        mChannels = 2;
        mBaseSamplerate = RATE;
        mLength = dmusic::Player(mDls, mSeg, RATE).length_seconds();
        if (dmusic::segment_loops(*mSeg)) setLooping(true);
        const char *base = std::strrchr(path, '/');
        std::fprintf(stderr, "[ff8-music] %s (%.0f s%s)\n", base ? base + 1 : path, mLength, dmusic::segment_loops(*mSeg) ? ", loops" : "");
        return SO_NO_ERROR;
    }
    AudioSourceInstance *createInstance() override;
    time getLength() { return mLength; }
};

class DmusicSegmentInstance : public AudioSourceInstance {
    DmusicSegment *mParent;
    dmusic::Player mPlayer;
    std::unique_ptr<float[]> mBuf;
    unsigned mBufFrames = 0;
    bool mEnded = false;
    size_t mFrames = 0;
    float mPeak = 0;
public:
    explicit DmusicSegmentInstance(DmusicSegment *p) : mParent(p), mPlayer(p->mDls, p->mSeg, DmusicSegment::RATE) {}
    unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize) override {
        if (mBufFrames < aSamplesToRead) { mBuf.reset(new float[2 * size_t(aSamplesToRead)]); mBufFrames = aSamplesToRead; }
        size_t n = mPlayer.render(mBuf.get(), aSamplesToRead, (mFlags & AudioSourceInstance::LOOPING) != 0);
        for (size_t j = 0; j < n; j++) {                      // interleaved -> SoLoud's planar
            aBuffer[j] = mBuf[2 * j];
            aBuffer[aBufferSize + j] = mBuf[2 * j + 1];
        }
        if (n < aSamplesToRead) mEnded = true;
        // one level report per playback, a few seconds in (Report a problem logs: is music audible)
        if (mFrames < 3 * DmusicSegment::RATE) {
            for (size_t j = 0; j < 2 * n; j++) mPeak = std::max(mPeak, std::fabs(mBuf[j]));
            mFrames += n;
            if (mFrames >= 3 * DmusicSegment::RATE) std::fprintf(stderr, "[ff8-music] playing, peak %.2f in the first 3 s\n", mPeak);
        }
        return unsigned(n);
    }
    result rewind() override { mPlayer.rewind(); mEnded = false; mStreamPosition = 0; return SO_NO_ERROR; }
    result seek(double aSeconds, float *, unsigned int) override {
        mPlayer.seek(aSeconds, (mFlags & AudioSourceInstance::LOOPING) != 0);
        mEnded = false;
        mStreamPosition = aSeconds;
        return SO_NO_ERROR;
    }
    bool hasEnded() override { return mEnded || mPlayer.ended(); }
};

AudioSourceInstance *DmusicSegment::createInstance() { return new DmusicSegmentInstance(this); }

}  // namespace SoLoud

/** FFNx NxAudioEngine::loadMusic: a segment as a SoLoud source, or nullptr (not a segment, or it
 *  did not load: FFNx then tries its own loaders). */
SoLoud::AudioSource *ffnx_ff8_load_segment(const char *path) {
    size_t n = std::strlen(path);
    if (n < 4 || strcasecmp(path + n - 4, ".sgt")) return nullptr;
    auto *s = new SoLoud::DmusicSegment();
    if (s->load(path) != SoLoud::SO_NO_ERROR) { delete s; return nullptr; }
    return s;
}
