// ff8_dmusic.cpp - see ff8_dmusic.h. Formats: Microsoft's DLS level 1 and DirectMusic's RIFF
// segment (DMSG) as documented in the DirectX SDK (dmusicf.h structures, DX6/DX7 sizes).
#include "ff8_dmusic.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>

namespace dmusic {

namespace {

constexpr int PPQ = 768;                     // DMUS_PPQ: music-time ticks per quarter note

uint32_t fourcc(const char *s) { return uint32_t(uint8_t(s[0])) | uint32_t(uint8_t(s[1])) << 8 | uint32_t(uint8_t(s[2])) << 16 | uint32_t(uint8_t(s[3])) << 24; }

struct Bytes {
    const uint8_t *p = nullptr;
    size_t n = 0;
    uint8_t u8(size_t o) const { return o < n ? p[o] : 0; }
    uint16_t u16(size_t o) const { return o + 2 <= n ? uint16_t(p[o] | p[o + 1] << 8) : 0; }
    int16_t s16(size_t o) const { return int16_t(u16(o)); }
    uint32_t u32(size_t o) const { return o + 4 <= n ? uint32_t(p[o]) | uint32_t(p[o + 1]) << 8 | uint32_t(p[o + 2]) << 16 | uint32_t(p[o + 3]) << 24 : 0; }
    int32_t s32(size_t o) const { return int32_t(u32(o)); }
    double f64(size_t o) const { double d = 0; if (o + 8 <= n) std::memcpy(&d, p + o, 8); return d; }
    Bytes sub(size_t o, size_t len) const { Bytes b; if (o <= n) { b.p = p + o; b.n = std::min(len, n - o); } return b; }
};

/** One chunk: id, and for RIFF/LIST the form type and the body after it. */
struct Chunk { uint32_t id = 0, type = 0; Bytes body; };

/** The chunks directly inside b (a RIFF/LIST body after its form type, or a file). */
std::vector<Chunk> chunks(Bytes b) {
    std::vector<Chunk> out;
    size_t o = 0;
    while (o + 8 <= b.n) {
        Chunk c;
        c.id = b.u32(o);
        uint32_t sz = b.u32(o + 4);
        Bytes body = b.sub(o + 8, sz);
        if (c.id == fourcc("RIFF") || c.id == fourcc("LIST")) { c.type = body.u32(0); c.body = body.sub(4, body.n >= 4 ? body.n - 4 : 0); }
        else c.body = body;
        out.push_back(c);
        o += 8 + size_t(sz) + (sz & 1);
    }
    return out;
}

const Chunk *find(const std::vector<Chunk> &cs, const char *id, const char *type = nullptr) {
    for (const Chunk &c : cs)
        if (c.id == fourcc(id) && (!type || c.type == fourcc(type))) return &c;
    return nullptr;
}

bool read_file(const std::string &path, std::vector<uint8_t> &out) {
    FILE *f = (std::fopen)(path.c_str(), "rb");   // a host path (not through FFNx's mapping)
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? size_t(n) : 0);
    bool ok = n >= 0 && std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

double timecents_s(int32_t scale) {           // DLS time: timecents * 65536; 0x80000000 = no time
    if (scale == INT32_MIN) return 0.0;
    return std::pow(2.0, scale / 65536.0 / 1200.0);
}

}  // namespace

// ============================================================================ DLS
struct Articulation {
    double attack = 0, hold = 0, decay = 0, release = 0.005, sustain = 1.0, delay = 0;   // EG1, seconds / 0..1
    double vel_to_attack_tc = 0, key_to_decay_tc = 0;   // timecents per full velocity / key range
    double lfo_hz = 5.0, lfo_delay = 0;
    double lfo_pitch_cents = 0, lfo_cc1_pitch_cents = 0, lfo_atten_db = 0;
    double pan = 0;                                     // -0.5 .. 0.5
    bool set = false;
};

struct Loop { bool on = false; uint32_t start = 0, length = 0; };

struct WaveSample {                               // a wave's own wsmp, or a region's
    int unity = 60;
    int fine_cents = 0;
    double gain_db = 0;
    Loop loop;
    bool set = false;
};

struct Wave {
    std::vector<float> pcm;                       // mono
    int rate = 22050;
    WaveSample ws;
};

struct Region {
    int key_lo = 0, key_hi = 127, vel_lo = 0, vel_hi = 127, key_group = 0;
    uint32_t wave = 0;
    WaveSample ws;                                // region's wsmp (if set, wins over the wave's)
    Articulation art;                             // region's art (if set, wins over the instrument's)
};

struct Instrument {
    uint32_t bank = 0, program = 0;               // ulBank (bit 31 drums, MSB bits 8-14, LSB 0-6)
    std::vector<Region> regions;
    Articulation art;
};

struct Dls {
    std::vector<Instrument> instruments;
    std::vector<Wave> waves;
    std::map<uint64_t, size_t> index;            // (drum, msb, lsb, program) -> instrument
    static uint64_t key(bool drum, int msb, int lsb, int prog) { return uint64_t(drum) << 24 | uint64_t(msb & 0x7F) << 16 | uint64_t(lsb & 0x7F) << 8 | uint64_t(prog & 0x7F); }
    const Instrument *find(bool drum, int msb, int lsb, int prog) const {
        auto it = index.find(key(drum, msb, lsb, prog));
        if (it == index.end()) it = index.find(key(drum, 0, 0, prog));        // bank fallback
        if (it == index.end() && drum) it = index.find(key(true, 0, 0, 0));   // any kit: the standard one
        return it == index.end() ? nullptr : &instruments[it->second];
    }
};

namespace {

void parse_wsmp(Bytes b, WaveSample &w) {
    uint32_t cb = b.u32(0);
    w.unity = b.u16(4);
    w.fine_cents = b.s16(6);
    w.gain_db = b.s32(8) / 655360.0;              // relative gain, 1/655360 dB
    uint32_t loops = b.u32(16);
    if (loops > 0) {
        Bytes l = b.sub(cb, 16);
        w.loop.on = true;                           // type 0 forward (DLS1 has only that)
        w.loop.start = l.u32(8);
        w.loop.length = l.u32(12);
        if (w.loop.length == 0) w.loop.on = false;
    }
    w.set = true;
}

void parse_art1(Bytes b, Articulation &a) {
    uint32_t cb = b.u32(0), n = b.u32(4);
    double vel_atk = 0, key_dec = 0;
    for (uint32_t i = 0; i < n; i++) {
        size_t o = cb + 12 * size_t(i);
        uint16_t src = b.u16(o), ctl = b.u16(o + 2), dst = b.u16(o + 4);
        int32_t sc = b.s32(o + 8);
        if (src == 0 && ctl == 0) {
            switch (dst) {
            case 0x206: a.attack = timecents_s(sc); break;        // EG1 attack
            case 0x207: a.decay = timecents_s(sc); break;         // EG1 decay
            case 0x209: a.release = std::max(0.005, timecents_s(sc)); break;
            case 0x20A: a.sustain = std::clamp(sc / 65536.0 / 1000.0, 0.0, 1.0); break;   // 0.1 %
            case 0x20B: a.delay = timecents_s(sc); break;         // EG1 delay (DLS2)
            case 0x20C: a.hold = timecents_s(sc); break;          // EG1 hold (DLS2)
            case 0x104: a.lfo_hz = 440.0 * std::pow(2.0, (sc / 65536.0 - 6900.0) / 1200.0); break;   // absolute pitch
            case 0x105: a.lfo_delay = timecents_s(sc); break;
            case 0x004: a.pan = std::clamp(sc / 65536.0 / 1000.0, -0.5, 0.5); break;   // 0.1 %
            }
        } else if (src == 1 && ctl == 0 && dst == 3) a.lfo_pitch_cents = sc / 65536.0;     // LFO -> pitch
        else if (src == 1 && ctl == 0x81 && dst == 3) a.lfo_cc1_pitch_cents = sc / 65536.0;   // LFO x mod wheel -> pitch
        else if (src == 1 && ctl == 0 && dst == 1) a.lfo_atten_db = sc / 65536.0 / 10.0;   // LFO -> attenuation (1/10 dB)
        else if (src == 2 && dst == 0x206) vel_atk = sc / 65536.0;                     // velocity -> attack time
        else if (src == 3 && dst == 0x207) key_dec = sc / 65536.0;                     // key -> decay time
    }
    a.vel_to_attack_tc = vel_atk;
    a.key_to_decay_tc = key_dec;
    a.set = true;
}

void parse_lart(Bytes b, Articulation &a) {
    for (const Chunk &c : chunks(b))
        if (c.id == fourcc("art1") || c.id == fourcc("art2")) parse_art1(c.body, a);
}

bool parse_wave(Bytes b, Wave &w) {
    std::vector<Chunk> cs = chunks(b);
    const Chunk *fmt = find(cs, "fmt "), *data = find(cs, "data"), *ws = find(cs, "wsmp");
    if (!fmt || !data) return false;
    uint16_t tag = fmt->body.u16(0), ch = fmt->body.u16(2), bits = fmt->body.u16(14);
    w.rate = int(fmt->body.u32(4));
    if (tag != 1 || ch < 1 || (bits != 8 && bits != 16) || w.rate <= 0) return false;
    size_t frame = size_t(ch) * (bits / 8), n = data->body.n / frame;
    w.pcm.resize(n);
    for (size_t i = 0; i < n; i++) {
        size_t o = i * frame;
        w.pcm[i] = bits == 8 ? (data->body.u8(o) - 128) / 128.0f : data->body.s16(o) / 32768.0f;   // first channel
    }
    if (ws) parse_wsmp(ws->body, w.ws);
    return true;
}

}  // namespace

std::shared_ptr<const Dls> load_dls(const std::string &path, std::string *err) {
    std::vector<uint8_t> file;
    if (!read_file(path, file)) { if (err) *err = "cannot read " + path; return nullptr; }
    Bytes all{file.data(), file.size()};
    std::vector<Chunk> top = chunks(all);
    if (top.empty() || top[0].id != fourcc("RIFF") || top[0].type != fourcc("DLS ")) { if (err) *err = "not a DLS file"; return nullptr; }
    std::vector<Chunk> body = chunks(top[0].body);
    auto dls = std::make_shared<Dls>();
    // the wave pool: ptbl offsets are from just after the wvpl list's form type
    const Chunk *wvpl = find(body, "LIST", "wvpl"), *ptbl = find(body, "ptbl");
    if (!wvpl || !ptbl) { if (err) *err = "DLS without a wave pool"; return nullptr; }
    Bytes pool = wvpl->body;
    uint32_t cues = ptbl->body.u32(4), cb = ptbl->body.u32(0);
    dls->waves.resize(cues);
    for (uint32_t i = 0; i < cues; i++) {
        uint32_t off = ptbl->body.u32(cb + 4 * size_t(i));
        Bytes at = pool.sub(off, pool.n - std::min<size_t>(off, pool.n));
        std::vector<Chunk> one = chunks(at.sub(0, 8 + size_t(at.u32(4))));
        if (!one.empty() && one[0].id == fourcc("LIST") && one[0].type == fourcc("wave")) parse_wave(one[0].body, dls->waves[i]);
    }
    const Chunk *lins = find(body, "LIST", "lins");
    if (!lins) { if (err) *err = "DLS without instruments"; return nullptr; }
    for (const Chunk &ic : chunks(lins->body)) {
        if (ic.type != fourcc("ins ")) continue;
        std::vector<Chunk> ins = chunks(ic.body);
        const Chunk *insh = find(ins, "insh");
        if (!insh) continue;
        Instrument in;
        in.bank = insh->body.u32(4);
        in.program = insh->body.u32(8) & 0x7F;
        if (const Chunk *la = find(ins, "LIST", "lart")) parse_lart(la->body, in.art);
        else if (const Chunk *l2 = find(ins, "LIST", "lar2")) parse_lart(l2->body, in.art);
        if (const Chunk *lr = find(ins, "LIST", "lrgn")) {
            for (const Chunk &rc : chunks(lr->body)) {
                if (rc.type != fourcc("rgn ") && rc.type != fourcc("rgn2")) continue;
                std::vector<Chunk> rg = chunks(rc.body);
                const Chunk *rgnh = find(rg, "rgnh"), *wlnk = find(rg, "wlnk");
                if (!rgnh || !wlnk) continue;
                Region r;
                r.key_lo = rgnh->body.u16(0); r.key_hi = rgnh->body.u16(2);
                r.vel_lo = rgnh->body.u16(4); r.vel_hi = rgnh->body.u16(6);
                if (r.vel_hi == 0) r.vel_hi = 127;
                r.key_group = rgnh->body.u16(10);
                r.wave = wlnk->body.u32(8);
                if (r.wave >= dls->waves.size()) continue;
                if (const Chunk *ws = find(rg, "wsmp")) parse_wsmp(ws->body, r.ws);
                if (const Chunk *la = find(rg, "LIST", "lart")) parse_lart(la->body, r.art);
                else if (const Chunk *l2 = find(rg, "LIST", "lar2")) parse_lart(l2->body, r.art);
                in.regions.push_back(r);
            }
        }
        bool drum = in.bank >> 31;
        uint64_t k = Dls::key(drum, (in.bank >> 8) & 0x7F, in.bank & 0x7F, int(in.program));
        if (!dls->index.count(k)) dls->index[k] = dls->instruments.size();
        dls->instruments.push_back(std::move(in));
    }
    if (std::getenv("DMUSIC_DEBUG")) {
        size_t wv = 0; for (auto &w : dls->waves) wv += !w.pcm.empty();
        std::fprintf(stderr, "dls: %zu instruments, %zu/%zu waves\n", dls->instruments.size(), wv, dls->waves.size());
    }
    return dls;
}

// ============================================================================ segment
namespace {
enum EvKind : uint8_t { EV_NOTE, EV_CC, EV_PROGRAM, EV_BEND, EV_BAND };

struct Ev {
    int32_t tick;
    EvKind kind;
    uint8_t a = 0, b = 0;      // note key, velocity / cc, value / program
    uint16_t v = 0;            // bend 0..16383 / band index
    uint32_t pch = 0;
    int32_t dur = 0;           // notes: length in ticks
};

struct BandInst {
    uint32_t pch, patch, flags;
    uint8_t pan, volume;
    int16_t transpose;
};
}  // namespace

struct Segment {
    std::vector<Ev> events;                       // by tick
    std::vector<std::vector<BandInst>> bands;
    std::vector<std::pair<int32_t, double>> tempo{{0, 120.0}};   // tick -> bpm
    int32_t length = PPQ * 4;
    int32_t loop_start = 0, loop_end = 0;
    uint32_t repeats = 0;
};

bool segment_loops(const Segment &s) { return s.repeats > 0; }

namespace {

void parse_seqt(Bytes b, Segment &s) {
    for (const Chunk &c : chunks(b)) {
        uint32_t isz = c.body.u32(0);
        if (isz == 0) continue;
        size_t n = (c.body.n - 4) / isz;
        if (c.id == fourcc("evtl")) {
            for (size_t i = 0; i < n; i++) {
                Bytes it = c.body.sub(4 + i * isz, isz);
                Ev e;
                e.tick = it.s32(0) + it.s16(12);
                e.pch = it.u32(8);
                uint8_t st = it.u8(14) & 0xF0, b1 = it.u8(15), b2 = it.u8(16);
                switch (st) {
                case 0x90: if (b2 == 0) continue; e.kind = EV_NOTE; e.a = b1; e.b = b2; e.dur = std::max(1, it.s32(4)); break;
                case 0xB0: e.kind = EV_CC; e.a = b1; e.b = b2; break;
                case 0xC0: e.kind = EV_PROGRAM; e.a = b1; break;
                case 0xE0: e.kind = EV_BEND; e.v = uint16_t(b1 | b2 << 7); break;
                default: continue;
                }
                s.events.push_back(e);
            }
        } else if (c.id == fourcc("curl")) {
            for (size_t i = 0; i < n; i++) {
                Bytes it = c.body.sub(4 + i * isz, isz);
                int32_t start = it.s32(0) + it.s16(16), dur = it.s32(4), reset_dur = it.s32(8);
                uint32_t pch = it.u32(12);
                int sv = it.s16(18), ev = it.s16(20), rv = it.s16(22);
                uint8_t type = it.u8(24), shape = it.u8(25), cc = it.u8(26), flags = it.u8(27);
                if (type != 3 && type != 4) continue;                 // pitch bend, CC
                auto emit = [&](int32_t t, int val) {
                    Ev e; e.tick = t; e.pch = pch;
                    if (type == 3) { e.kind = EV_BEND; e.v = uint16_t(std::clamp(val, 0, 16383)); }
                    else { e.kind = EV_CC; e.a = cc; e.b = uint8_t(std::clamp(val, 0, 127)); }
                    s.events.push_back(e);
                };
                if (shape == 1 || dur <= 0) emit(start + std::max(0, dur), ev);   // instant
                else {
                    int steps = std::clamp(dur / 32, 1, 256);
                    for (int k = 0; k <= steps; k++) {
                        double x = double(k) / steps, y;
                        switch (shape) {
                        case 2: y = x * x; break;                                 // exponential
                        case 3: y = std::sqrt(x); break;                          // logarithmic
                        case 4: y = 0.5 - 0.5 * std::cos(x * 3.14159265358979); break;   // sine
                        default: y = x; break;                                    // linear
                        }
                        emit(start + int32_t(dur * x), int(std::lround(sv + (ev - sv) * y)));
                    }
                }
                if ((flags & 1) && reset_dur > 0) emit(start + dur + reset_dur, rv);   // DMUS_CURVE_RESET
            }
        }
    }
}

void parse_band_list(Bytes b, Segment &s) {           // LIST lbdl -> LIST lbnd (bdih|bd2h, RIFF DMBD)
    for (const Chunk &l : chunks(b)) {
        if (l.type != fourcc("lbnd")) continue;
        std::vector<Chunk> lb = chunks(l.body);
        int32_t t = 0;
        if (const Chunk *h = find(lb, "bdih")) t = h->body.s32(0);
        else if (const Chunk *h2 = find(lb, "bd2h")) t = h2->body.s32(0);
        const Chunk *dmbd = find(lb, "RIFF", "DMBD");
        if (!dmbd) continue;
        std::vector<BandInst> band;
        if (const Chunk *lbil = find(chunks(dmbd->body), "LIST", "lbil")) {
            for (const Chunk &li : chunks(lbil->body)) {
                if (li.type != fourcc("lbin")) continue;
                const Chunk *bins = find(chunks(li.body), "bins");
                if (!bins) continue;
                BandInst bi;
                bi.patch = bins->body.u32(0);
                bi.pch = bins->body.u32(24);
                bi.flags = bins->body.u32(28);
                bi.pan = bins->body.u8(32);
                bi.volume = bins->body.u8(33);
                bi.transpose = bins->body.s16(34);
                band.push_back(bi);
            }
        }
        Ev e; e.tick = std::max(0, t); e.kind = EV_BAND; e.v = uint16_t(s.bands.size());
        s.bands.push_back(std::move(band));
        s.events.push_back(e);
    }
}

}  // namespace

std::shared_ptr<const Segment> load_segment(const std::string &path, std::string *err) {
    std::vector<uint8_t> file;
    if (!read_file(path, file)) { if (err) *err = "cannot read " + path; return nullptr; }
    std::vector<Chunk> top = chunks(Bytes{file.data(), file.size()});
    if (top.empty() || top[0].type != fourcc("DMSG")) { if (err) *err = "not a DirectMusic segment"; return nullptr; }
    std::vector<Chunk> body = chunks(top[0].body);
    auto s = std::make_shared<Segment>();
    if (const Chunk *h = find(body, "segh")) {
        s->repeats = h->body.u32(0);
        s->length = std::max<int32_t>(1, h->body.s32(4));
        // DX6 header: repeats, length, play start, loop start, loop end, resolution
        if (h->body.n >= 20) { s->loop_start = h->body.s32(12); s->loop_end = h->body.s32(16); }
    }
    if (s->loop_end <= s->loop_start || s->loop_end > s->length) { s->loop_start = 0; s->loop_end = s->length; }
    if (const Chunk *trkl = find(body, "LIST", "trkl")) {
        for (const Chunk &tk : chunks(trkl->body)) {
            if (tk.type != fourcc("DMTK")) continue;
            for (const Chunk &c : chunks(tk.body)) {
                if (c.id == fourcc("seqt")) parse_seqt(c.body, *s);
                else if (c.id == fourcc("tetr")) {
                    uint32_t isz = c.body.u32(0);
                    if (isz >= 16) {
                        s->tempo.clear();
                        for (size_t o = 4; o + isz <= c.body.n; o += isz) s->tempo.emplace_back(c.body.s32(o), c.body.f64(o + 8));
                        if (s->tempo.empty() || s->tempo[0].first > 0) s->tempo.insert(s->tempo.begin(), {0, s->tempo.empty() ? 120.0 : s->tempo[0].second});
                    }
                } else if (c.id == fourcc("RIFF") && c.type == fourcc("DMBT")) {
                    if (const Chunk *lbdl = find(chunks(c.body), "LIST", "lbdl")) parse_band_list(lbdl->body, *s);
                }
            }
        }
    }
    for (auto &t : s->tempo) if (!(t.second > 1.0 && t.second < 1000.0)) t.second = 120.0;
    if (std::getenv("DMUSIC_DEBUG"))
        std::fprintf(stderr, "sgt: %zu events, %zu bands, tempo %.1f, length %d, loop %d-%d x%u\n", s->events.size(), s->bands.size(),
                     s->tempo[0].second, s->length, s->loop_start, s->loop_end, s->repeats);
    std::stable_sort(s->events.begin(), s->events.end(), [](const Ev &a, const Ev &b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return (a.kind == EV_BAND) > (b.kind == EV_BAND);          // bands first, then the rest in order
    });
    return s;
}

// ============================================================================ player
namespace {

struct Channel {
    const Instrument *inst = nullptr;
    int msb = 0, lsb = 0, program = 0;
    bool drum = false;
    int volume = 100, expression = 127, pan = 64, mod = 0;
    int bend = 8192, bend_range = 2;
    int transpose = 0;
    bool sustain = false;
    int rpn_msb = 127, rpn_lsb = 127;
};

struct Voice {
    bool active = false;
    uint32_t pch = 0;
    int key = 0;
    const Region *rgn = nullptr;
    const Wave *wave = nullptr;
    const Articulation *art = nullptr;
    double pos = 0, base_step = 0;          // samples; step at no bend
    Loop loop;
    double gain = 0;                        // velocity * region gain
    int stage = 0;                          // 0 delay, 1 attack, 2 hold, 3 decay, 4 sustain, 5 release
    double eg = 0, t_stage = 0;             // EG level 0..1, time in stage
    double attack = 0, decay = 0;           // with velocity / key scaling
    double lfo_t = 0;
    bool released = false, held = false;    // held: released while the sustain pedal is down
    int64_t off_tick = 0;                   // when the note ends (ticks of play)
    uint64_t age = 0;
};

constexpr int MAX_VOICES = 96;

}  // namespace

struct Player::Impl {
    std::shared_ptr<const Dls> dls;
    std::shared_ptr<const Segment> seg;
    int rate;
    std::map<uint32_t, Channel> ch;
    Voice voices[MAX_VOICES];
    uint64_t next_age = 0;
    double tick = 0;                        // ticks in the segment
    int64_t played = 0;                     // ticks of play (loops counted): note-off clock
    double play_tick = 0;
    size_t ev = 0;
    bool done = false;
    double tail = 0;                        // seconds since the end

    Impl(std::shared_ptr<const Dls> d, std::shared_ptr<const Segment> s, int r) : dls(std::move(d)), seg(std::move(s)), rate(r) { reset(); }

    void reset() {
        ch.clear();
        for (Voice &v : voices) v.active = false;
        tick = 0; play_tick = 0; ev = 0; done = false; tail = 0;
    }

    double bpm_at(double t) const {
        double b = seg->tempo[0].second;
        for (auto &x : seg->tempo) { if (x.first <= t) b = x.second; else break; }
        return b;
    }

    Channel &chan(uint32_t p) {
        auto it = ch.find(p);
        if (it != ch.end()) return it->second;
        Channel c;
        c.drum = (p % 16) == 9;
        c.inst = dls ? dls->find(c.drum, 0, 0, 0) : nullptr;
        return ch.emplace(p, c).first->second;
    }

    void set_program(Channel &c) { c.inst = dls ? dls->find(c.drum, c.msb, c.lsb, c.program) : nullptr; }

    void apply_band(const std::vector<BandInst> &band) {
        for (const BandInst &b : band) {
            Channel &c = chan(b.pch);
            if (b.flags & 0x1) {                                   // DMUS_IO_INST_PATCH
                c.drum = b.patch >> 31;
                c.msb = (b.patch >> 16) & 0x7F; c.lsb = (b.patch >> 8) & 0x7F; c.program = b.patch & 0x7F;
            } else if (b.pch % 16 == 9) { c.drum = true; c.msb = c.lsb = c.program = 0; }
            set_program(c);
            if (b.flags & 0x20) c.pan = b.pan;                       // PAN
            if (b.flags & 0x40) c.volume = b.volume;                 // VOLUME
            if (b.flags & 0x80) c.transpose = b.transpose;           // TRANSPOSE
        }
    }

    void release(Voice &v) {
        if (v.released) return;
        if (chan(v.pch).sustain) { v.held = true; return; }
        v.released = true; v.stage = 5; v.t_stage = 0;
    }

    void note_on(uint32_t pch, int key, int vel, int32_t dur) {
        Channel &c = chan(pch);
        if (!c.inst) return;
        int k = std::clamp(key + (c.drum ? 0 : c.transpose), 0, 127);
        for (const Region &r : c.inst->regions) {
            if (k < r.key_lo || k > r.key_hi || vel < r.vel_lo || vel > r.vel_hi) continue;
            if (r.key_group)                                             // exclusive class: cut the others
                for (Voice &o : voices) if (o.active && o.pch == pch && o.rgn && o.rgn->key_group == r.key_group) o.active = false;
            const Wave &w = dls->waves[r.wave];
            if (w.pcm.empty()) continue;
            Voice *v = nullptr;
            for (Voice &x : voices) if (!x.active) { v = &x; break; }
            if (!v) {                                                    // steal: released first, then oldest
                for (Voice &x : voices) if (!v || (x.released && !v->released) || (x.released == v->released && x.age < v->age)) v = &x;
            }
            const WaveSample &ws = r.ws.set ? r.ws : w.ws;
            const Articulation &a = r.art.set ? r.art : c.inst->art;
            *v = Voice();
            v->active = true; v->pch = pch; v->key = k; v->rgn = &r; v->wave = &w; v->art = &a;
            v->loop = ws.loop;
            double semis = (k - ws.unity) + ws.fine_cents / 100.0;
            v->base_step = std::pow(2.0, semis / 12.0) * w.rate / rate;
            double velg = double(vel) / 127.0;
            v->gain = velg * velg * std::pow(10.0, ws.gain_db / 20.0);
            v->attack = a.attack * std::pow(2.0, a.vel_to_attack_tc * (vel / 128.0) / 1200.0);
            v->decay = a.decay * std::pow(2.0, a.key_to_decay_tc * (k / 128.0) / 1200.0);
            v->stage = a.delay > 0 ? 0 : 1;
            v->off_tick = int64_t(play_tick) + dur;
            v->age = next_age++;
        }
    }

    void cc(uint32_t pch, int n, int val) {
        Channel &c = chan(pch);
        switch (n) {
        case 0: c.msb = val; break;
        case 32: c.lsb = val; break;
        case 1: c.mod = val; break;
        case 7: c.volume = val; break;
        case 10: c.pan = val; break;
        case 11: c.expression = val; break;
        case 64:
            c.sustain = val >= 64;
            if (!c.sustain) for (Voice &v : voices) if (v.active && v.pch == pch && v.held) { v.held = false; release(v); }
            break;
        case 101: c.rpn_msb = val; break;
        case 100: c.rpn_lsb = val; break;
        case 6: if (c.rpn_msb == 0 && c.rpn_lsb == 0) c.bend_range = val; break;   // pitch bend range
        case 120: case 123:                                                          // all sound / notes off
            for (Voice &v : voices) if (v.active && v.pch == pch) { if (n == 120) v.active = false; else release(v); }
            break;
        case 121: c.mod = 0; c.expression = 127; c.bend = 8192; c.sustain = false; break;
        }
    }

    void dispatch(const Ev &e, bool sound) {
        switch (e.kind) {
        case EV_NOTE: if (sound) note_on(e.pch, e.a, e.b, e.dur); break;
        case EV_CC: cc(e.pch, e.a, e.b); break;
        case EV_PROGRAM: { Channel &c = chan(e.pch); c.program = e.a; set_program(c); break; }
        case EV_BEND: chan(e.pch).bend = e.v; break;
        case EV_BAND: if (e.v < seg->bands.size()) apply_band(seg->bands[e.v]); break;
        }
    }

    /** Events up to tick t (exclusive); jumps back at the loop end. Returns false at the end. */
    bool advance_to(double t, bool loop, bool sound) {
        const int32_t end = loop && seg->repeats ? seg->loop_end : seg->length;
        while (true) {
            while (ev < seg->events.size() && seg->events[ev].tick < std::min<double>(t, end)) dispatch(seg->events[ev++], sound);
            if (t < end) { tick = t; return true; }
            if (!(loop && seg->repeats)) { tick = end; return false; }
            t -= (seg->loop_end - seg->loop_start);
            ev = size_t(std::lower_bound(seg->events.begin(), seg->events.end(), seg->loop_start,
                                         [](const Ev &e, int32_t x) { return e.tick < x; }) - seg->events.begin());
            // the loop start's bands and settings stay as the loop end left them, as DirectMusic does
        }
    }

    void render(float *out, size_t frames, bool loop, size_t &written) {
        const size_t BLOCK = 64;
        written = 0;
        while (written < frames) {
            size_t n = std::min(BLOCK, frames - written);
            if (!done) {
                double bpm = bpm_at(tick);
                double dt = bpm * PPQ / 60.0 / rate * n;
                if (!advance_to(tick + dt, loop, true)) done = true;
                play_tick += dt;
            }
            // note-offs on the play clock
            for (Voice &v : voices) if (v.active && !v.released && !v.held && play_tick >= double(v.off_tick)) release(v);
            float *o = out + 2 * written;
            std::fill(o, o + 2 * n, 0.0f);
            bool any = false;
            for (Voice &v : voices) if (v.active) { any = true; run_voice(v, o, n); }
            written += n;
            if (done) {
                tail += double(n) / rate;
                if (!any || tail > 4.0) return;
            }
        }
    }

    void run_voice(Voice &v, float *o, size_t n) {
        const Channel &c = chan(v.pch);
        const Articulation &a = *v.art;
        const std::vector<float> &pcm = v.wave->pcm;
        double chg = (c.volume / 127.0) * (c.volume / 127.0) * (c.expression / 127.0) * (c.expression / 127.0);
        double pan = std::clamp((c.pan - 64) / 64.0 + a.pan * 2.0, -1.0, 1.0);
        double gl = std::cos((pan + 1.0) * 0.25 * 3.14159265358979), gr = std::sin((pan + 1.0) * 0.25 * 3.14159265358979);
        double bend = (c.bend - 8192) / 8192.0 * c.bend_range * 100.0;   // cents
        double vib_depth = a.lfo_pitch_cents + a.lfo_cc1_pitch_cents * (c.mod / 127.0);
        const double dt = 1.0 / rate;
        for (size_t i = 0; i < n; i++) {
            // EG1
            double amp;
            switch (v.stage) {
            case 0: v.eg = 0; if (v.t_stage >= a.delay) { v.stage = 1; v.t_stage = 0; } break;
            case 1:
                v.eg = v.attack > 0 ? std::min(1.0, v.t_stage / v.attack) : 1.0;
                if (v.eg >= 1.0) { v.stage = a.hold > 0 ? 2 : 3; v.t_stage = 0; }
                break;
            case 2: v.eg = 1.0; if (v.t_stage >= a.hold) { v.stage = 3; v.t_stage = 0; } break;
            case 3:
                v.eg = v.decay > 0 ? 1.0 - v.t_stage / v.decay : a.sustain;    // full scale (96 dB) per decay time
                if (v.eg <= a.sustain) { v.eg = a.sustain; v.stage = 4; }
                break;
            case 4: break;
            case 5: v.eg -= dt / std::max(0.005, a.release); break;          // full scale per release time
            }
            v.t_stage += dt;
            if (v.stage == 5 && v.eg <= 0.0) { v.active = false; return; }
            if (v.stage == 1 || v.stage == 0) amp = v.eg;                     // attack: linear amplitude
            else amp = v.eg >= 1.0 ? 1.0 : std::pow(10.0, -96.0 * (1.0 - std::max(0.0, v.eg)) / 20.0);
            if (v.stage == 5 && amp < 1e-4) { v.active = false; return; }
            // LFO
            double cents = bend, atten = 0;
            if (vib_depth != 0 || a.lfo_atten_db != 0) {
                v.lfo_t += dt;
                if (v.lfo_t > a.lfo_delay) {
                    double l = std::sin(2.0 * 3.14159265358979 * a.lfo_hz * (v.lfo_t - a.lfo_delay));
                    cents += l * vib_depth;
                    atten = l * a.lfo_atten_db;
                }
            }
            double step = v.base_step * (cents != 0 ? std::pow(2.0, cents / 1200.0) : 1.0);
            // sample
            size_t ip = size_t(v.pos);
            if (ip >= pcm.size()) { v.active = false; return; }
            double fr = v.pos - double(ip);
            size_t ip1 = ip + 1;
            if (v.loop.on && ip1 >= v.loop.start + v.loop.length) ip1 = v.loop.start;
            float s0 = pcm[ip], s1 = ip1 < pcm.size() ? pcm[ip1] : 0.0f;
            double s = s0 + (s1 - s0) * fr;
            double g = s * amp * v.gain * chg * (atten != 0 ? std::pow(10.0, atten / 20.0) : 1.0);
            o[2 * i] += float(g * gl);
            o[2 * i + 1] += float(g * gr);
            v.pos += step;
            if (v.loop.on) {
                double le = double(v.loop.start) + v.loop.length;
                while (v.pos >= le && le <= pcm.size()) v.pos -= v.loop.length;
            }
        }
    }
};

Player::Player(std::shared_ptr<const Dls> dls, std::shared_ptr<const Segment> seg, int rate)
    : p_(new Impl(std::move(dls), std::move(seg), rate)), rate_(rate) {}
Player::~Player() = default;

size_t Player::render(float *out, size_t frames, bool loop) {
    size_t w = 0;
    p_->render(out, frames, loop, w);
    // headroom: many voices at full volume; a soft knee above 0.8 instead of hard clipping
    for (size_t i = 0; i < 2 * w; i++) {
        float x = out[i] * 0.75f, ax = std::fabs(x);
        if (ax > 0.85f) x = std::copysign(0.85f + 0.15f * std::tanh((ax - 0.85f) / 0.15f), x);
        out[i] = x;
    }
    return w;
}

void Player::rewind() { p_->reset(); }

void Player::seek(double seconds, bool loop) {
    p_->reset();
    double rem = seconds;
    while (rem > 0 && !p_->done) {                      // settings only: notes before the point are not sounded
        double bpm = p_->bpm_at(p_->tick);
        double step = std::min(rem, 0.05);
        double dt = bpm * PPQ / 60.0 * step;
        if (!p_->advance_to(p_->tick + dt, loop, false)) p_->done = true;
        p_->play_tick += dt;
        rem -= step;
    }
}

double Player::length_seconds() const {
    const Segment &s = *p_->seg;
    double sec = 0;
    for (size_t i = 0; i < s.tempo.size(); i++) {
        int32_t a = s.tempo[i].first, b = i + 1 < s.tempo.size() ? s.tempo[i + 1].first : s.length;
        if (a >= s.length) break;
        b = std::min(b, s.length);
        sec += double(b - a) / PPQ * 60.0 / s.tempo[i].second;
    }
    return sec;
}

bool Player::ended() const {
    if (!p_->done) return false;
    for (const Voice &v : p_->voices) if (v.active) return p_->tail > 4.0;
    return true;
}

}  // namespace dmusic
