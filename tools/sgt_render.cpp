// Render an FF8 DirectMusic segment to a WAV with FF8Recomp's player (src/ffnx_port/dmusic),
// to check it on the PC.
//   clang++ -std=c++20 -O2 -I src/ffnx_port/dmusic tools/sgt_render.cpp src/ffnx_port/dmusic/ff8_dmusic.cpp -o build/sgt_render.exe
//   build/sgt_render.exe <FF8.dls> <segment.sgt> <out.wav> [seconds=40]
#include "ff8_dmusic.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: sgt_render <dls> <sgt> <out.wav> [seconds]\n"); return 2; }
    std::string err;
    auto dls = dmusic::load_dls(argv[1], &err);
    if (!dls) { std::fprintf(stderr, "dls: %s\n", err.c_str()); return 1; }
    auto seg = dmusic::load_segment(argv[2], &err);
    if (!seg) { std::fprintf(stderr, "sgt: %s\n", err.c_str()); return 1; }
    const int rate = 44100;
    double secs = argc > 4 ? std::atof(argv[4]) : 40.0;
    dmusic::Player p(dls, seg, rate);
    bool loop = dmusic::segment_loops(*seg);
    std::vector<float> all;
    std::vector<float> buf(2 * 1024);
    size_t want = size_t(secs * rate);
    while (all.size() / 2 < want) {
        size_t n = p.render(buf.data(), 1024, loop);
        all.insert(all.end(), buf.begin(), buf.begin() + 2 * n);
        if (n < 1024) break;
    }
    double peak = 0, sum = 0;
    size_t clip = 0;
    for (float x : all) { peak = std::fmax(peak, std::fabs(x)); sum += double(x) * x; if (std::fabs(x) > 0.79f) clip++; }
    std::printf("length %.1f s (one pass %.1f s, loops %d), rendered %.1f s, peak %.3f, rms %.4f, above 0.79: %zu\n",
                p.length_seconds(), p.length_seconds(), loop, all.size() / 2.0 / rate, peak, std::sqrt(sum / std::max<size_t>(1, all.size())), clip);
    FILE *f = std::fopen(argv[3], "wb");
    if (!f) return 1;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    uint32_t data = uint32_t(all.size() * 2);
    std::fwrite("RIFF", 1, 4, f); w32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(rate); w32(rate * 4); w16(4); w16(16);
    std::fwrite("data", 1, 4, f); w32(data);
    for (float x : all) { int v = int(std::lround(std::fmax(-1.0, std::fmin(1.0, x)) * 32767)); w16(uint16_t(int16_t(v))); }
    std::fclose(f);
    return 0;
}
