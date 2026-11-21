// ff8_dmusic.h - FF8's DirectMusic music without DirectMusic: the 2013 release's segments
// (Data/Music/dmusic/*.sgt) played through its DLS instrument collection (FF8.dls) by a small
// wavetable synthesizer. Android has no DirectMusic; FFNx plays FF8's music by name through its
// own audio engine (use_external_music), and sgt_source.cpp hands it these as SoLoud sources.
//
// Portable C++ (no Android, no SoLoud): tools/sgt_render.cpp renders a segment to a WAV on the PC.
//
// What is played: the segment's sequence track (notes, controllers, pitch bend, curves), tempo
// track, band track (programs, volume, pan, transpose per performance channel) and its loop
// (segh: repeats, loop start and end). Voices: DLS level 1 regions, sample loops, the volume
// envelope (EG1), LFO vibrato, pan, velocity, channel volume/expression, sustain pedal, key
// groups. Not played: chord/style/motif tracks (FF8's segments are sequences), DLS filters
// (level 2), reverb and chorus.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dmusic {

struct Dls;        // an instrument collection (FF8.dls)
struct Segment;    // a segment (*.sgt)

std::shared_ptr<const Dls> load_dls(const std::string &path, std::string *err);
std::shared_ptr<const Segment> load_segment(const std::string &path, std::string *err);

/** Whether the segment asks to loop (segh repeats > 0). */
bool segment_loops(const Segment &s);

/** Plays one segment. Not thread-safe; one per playing instance. */
class Player {
public:
    Player(std::shared_ptr<const Dls> dls, std::shared_ptr<const Segment> seg, int rate);
    ~Player();
    /** Interleaved stereo float frames; returns how many were written (fewer when the segment
     *  has ended: it does not loop, its last note is over). */
    size_t render(float *out, size_t frames, bool loop);
    void rewind();
    /** Jump to a time (seconds of play, loops counted), notes from before it are not sounded. */
    void seek(double seconds, bool loop);
    /** One pass through the segment, in seconds (its tempo map applied). */
    double length_seconds() const;
    bool ended() const;
    int rate() const { return rate_; }
private:
    struct Impl;
    std::unique_ptr<Impl> p_;
    int rate_;
};

}  // namespace dmusic
