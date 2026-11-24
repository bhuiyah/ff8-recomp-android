// GitHub #8 (comment of 2026-11-24): mod textures of different sizes on one page (Lunar Cry Plus,
// Horizon Pack Plus). Compiles ModdedTexture::drawImage as it is in the FFNx tree
// (drawimage.inc, cut out of external/FFNx/src/ff8/mod.cpp by run.sh) and checks every read.
#include <cstdint>
#include <cstdio>
#include <vector>

static long outOfBounds;
static const uint32_t *lo, *hi;
// A pointer to the source image that counts reads outside it.
struct Src {
    const uint32_t *p;
    Src(const uint32_t *q) : p(q) {}
    Src operator+(long n) const { return Src(p + n); }
    Src &operator+=(long n) { p += n; return *this; }
    uint32_t operator*() const { if (p < lo || p >= hi) { ++outOfBounds; return 0; } return *p; }
    uint32_t operator[](long n) const { return *(*this + n); }
};
struct ModdedTexture { static void drawImage(Src, int, uint8_t, uint32_t *, int, uint8_t, int, int, int, int, int, int); };
#include "drawimage.inc"

int main() {
    int bad = 0;
    const int w = 16, h = 8;
    for (int s = 1; s <= 16; ++s) for (int t = s; t <= 16; ++t) {
        std::vector<uint32_t> src(w * s * h * s), dst(w * t * h * t);
        for (size_t i = 0; i < src.size(); ++i) src[i] = 0x7F000000u | uint32_t(i);
        lo = src.data(); hi = lo + src.size(); outOfBounds = 0;
        ModdedTexture::drawImage(Src(src.data()), w, uint8_t(s), dst.data(), w, uint8_t(t), 0, 0, w, h, 0, 0);
        long wrong = 0;   // nearest source pixel of the whole image
        for (int y = 0; y < h * t; ++y) for (int x = 0; x < w * t; ++x)
            if (dst[y * w * t + x] != src[(y * s / t) * w * s + x * s / t]) ++wrong;
        if (outOfBounds || wrong) { ++bad; std::printf("%dx on a %dx page: %ld reads outside, %ld wrong pixels\n", s, t, outOfBounds, wrong); }
    }
    std::printf(bad ? "%d FAILED\n" : "all passed\n", bad);
    return bad != 0;
}
