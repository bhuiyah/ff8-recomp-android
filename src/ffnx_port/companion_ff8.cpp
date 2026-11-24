// companion_ff8.cpp - FF8's live game state for the bottom screen (CompanionView), the FF8 half of
// companion.cpp's snapshot: the party (HP, level, junctions, magic stock), the GFs, items and
// Triple Triad cards, gil, play time and where the party is. Read from the game's own savemap
// (FFNx ff8/save_data.h, the game keeps it live) and its kernel.bin text (ff8/kernel.h).
#include <sys/uio.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "common.h"
#include "globals.h"
#include "ff8.h"
#include "ff8/save_data.h"
#include "ff8/kernel.h"
#include "ff8/engine.h"
#include "gamehacks.h"
#include "ff8/remaster.h"
#include "cfg.h"
#include <vector>
#include <cctype>

extern "C" void ff7_ss_location(char *out, unsigned n, uint32_t *play_seconds);   // savestate_ffnx.cpp
char *ff8_get_card_name(int32_t card_id);                                          // FFNx exe_data.cpp

namespace {

// FF8 text (its own encoding, 0-terminated) to JSON-safe text.
std::string text(const char *enc, size_t max = 64) {
    if (!enc) return "";
    char buf[96];
    size_t n = 0;
    while (n < max && n + 1 < sizeof buf && enc[n]) { buf[n] = enc[n]; n++; }
    buf[n] = 0;
    std::string s = ff8_decode_text(buf), out;
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if ((unsigned char)c >= 0x20) out += c;
    }
    return out;
}

const ff8_kernel_vanilla_data *kernel() { return ff8_externals.kernel; }

const char *ktext(ff8_kernel_section sec, uint16_t off) {
    const ff8_kernel_vanilla_data *k = kernel();
    if (!k || off == 0xFFFF) return nullptr;
    return (const char *)k + k->header.section_offsets[(uint32_t)sec] + off;
}

std::string magic_name(unsigned id) {
    if (!kernel() || id == 0 || id >= 57) return "";
    return text(ktext(ff8_kernel_section::magic_text, kernel()->magic[id].offset_spell_name));
}

std::string item_name(unsigned id) {
    const ff8_kernel_vanilla_data *k = kernel();
    if (!k || id == 0) return "";
    // battle_items[0] is "no item"; the others begin with Tent (id 33)
    if (id <= 32) return text(ktext(ff8_kernel_section::battle_item_text, k->battle_items[id].offset_item_name));
    if (id - 33 < 166) return text(ktext(ff8_kernel_section::non_battle_item_text, k->non_battle_items[id - 33].offset_to_item_name));
    return "";
}

// The 11 playable characters' names (Squall and Rinoa as the player named them).
std::string char_name(const savemap_ff8 *sm, unsigned id) {
    static const char *const fixed[] = { "Squall", "Zell", "Irvine", "Quistis", "Rinoa", "Selphie", "Seifer", "Edea",
                                         "Laguna", "Kiros", "Ward" };
    if (id == 0) { std::string s = text((const char *)sm->header.squall_name, 12); if (!s.empty()) return s; }
    if (id == 4) { std::string s = text((const char *)sm->header.rinoa_name, 12); if (!s.empty()) return s; }
    return id < 11 ? fixed[id] : "?";
}

const char *mode_name(int m) {
    switch (m) {
    case MODE_FIELD: return "field";
    case MODE_BATTLE: return "battle";
    case MODE_WORLDMAP: return "world";
    case MODE_MENU: return "menu";
    case MODE_CARDGAME: return "cards";
    default: return "other";
    }
}

// Battle: the game's 11 battle slots (0xD0 bytes each: 3 party, 5 enemies, 3 GFs), as the
// FF8 modding wiki's BattleSlotData lays them out (FF8ModdingWiki TechnicalReference/Battle).
constexpr unsigned SLOT_SIZE = 0xD0;
struct battle_slot {
    const uint8_t *p;
    uint32_t u32(unsigned o) const { uint32_t v; memcpy(&v, p + o, 4); return v; }
    uint16_t u16(unsigned o) const { uint16_t v; memcpy(&v, p + o, 2); return v; }
    uint32_t info() const { return u32(0x00); }        // the monster's info: its first dword points at its name (FFNx voice.cpp)
    uint32_t status2() const { return u32(0x08); }
    uint32_t max_atb() const { return u32(0x10); }
    uint32_t atb() const { return u32(0x14); }
    uint32_t hp() const { return u32(0x18); }
    uint32_t max_hp() const { return u32(0x1C); }
    uint16_t elem(int e) const { return u16(0x44 + 2 * e); }  // 800 neutral, lower takes more, 900 void, above absorbs
    uint16_t status1() const { return u16(0x80); }
    uint8_t com_id() const { return p[0xBB]; }           // 0xFF: an empty slot
    uint8_t level() const { return p[0xBC]; }
};
// Guest memory that may not be there: false instead of a fault.
bool safe_read(uint32_t va, void *out, size_t n) {
    if (va < 0x10000) return false;
    struct iovec l = { out, n }, r = { (void *)(uintptr_t)va, n };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == (ssize_t)n;
}
battle_slot slot_at(unsigned i) {
    return { (const uint8_t *)ff8_externals.battle_char_struct_dword_1D27B10 + i * SLOT_SIZE };
}

// ---- place names: the game's menu/areames.dc1 (what its save list shows), by the savemap's
// location id: a u16 count, u16 offsets, FF8 text. 0x0E nn in it is a name from main's
// namedic.bin (the same layout, nn - 0x20: Galbadia, Esthar, ... Lunar Base, Ultimecia Castle).
std::vector<uint8_t> lzss(const std::vector<uint8_t> &src, size_t out_len) {
    std::vector<uint8_t> out; out.reserve(out_len);
    uint8_t win[4096] = {}; unsigned wp = 0xFEE; size_t i = 0;
    while (i < src.size() && out.size() < out_len) {
        const unsigned flags = src[i++];
        for (int b = 0; b < 8 && i < src.size() && out.size() < out_len; b++) {
            if (flags & (1u << b)) { const uint8_t c = src[i++]; out.push_back(c); win[wp] = c; wp = (wp + 1) & 0xFFF; }
            else {
                if (i + 1 >= src.size()) return out;
                const unsigned off = src[i] | ((src[i + 1] & 0xF0) << 4), len = (src[i + 1] & 0x0F) + 3;
                i += 2;
                for (unsigned k = 0; k < len && out.size() < out_len; k++) {
                    const uint8_t c = win[(off + k) & 0xFFF]; out.push_back(c); win[wp] = c; wp = (wp + 1) & 0xFFF;
                }
            }
        }
    }
    return out;
}

// The whole of a file of the game's language folder: the 2013 release's Data/lang-en, or the
// Remastered's main.zzz in FFNx's game_language.
bool read_lang_file(const char *name, std::vector<uint8_t> &out) {
    out.clear();
    if (ff8_remastered_edition) {
        static const char *const lang[] = { "en", "en", "fr", "de", "es", "it" };   // FFNx game_language
        char zname[64]; snprintf(zname, sizeof zname, "data\\lang-%s\\%s", lang[game_language >= 1 && game_language <= 5 ? game_language : 1], name);
        Zzz::File *f = g_FF8ZzzArchiveMain.openFile(zname);
        if (!f) return false;
        out.resize(f->size());
        const bool ok = out.empty() || f->read(out.data(), (unsigned)out.size()) == (int)out.size();
        Zzz::closeFile(f);
        return ok;
    }
    char path[1024]; snprintf(path, sizeof path, "%s/Data/lang-en/%s", basedir, name);
    std::FILE *f = (std::fopen)(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    if (n > 0) { out.resize((size_t)n); if (std::fread(out.data(), 1, out.size(), f) != out.size()) out.clear(); }
    std::fclose(f);
    return !out.empty();
}

// A file out of one of the game's archives (archive.fl names, .fi {size, offset, compression}, .fs),
// found by the end of its path ("\menu\areames.dc1").
std::vector<uint8_t> archive_file(const char *archive, const char *suffix) {
    std::vector<uint8_t> fl, fi, fs, out;
    const std::string a(archive);
    if (!read_lang_file((a + ".fl").c_str(), fl) || !read_lang_file((a + ".fi").c_str(), fi) || !read_lang_file((a + ".fs").c_str(), fs))
        return out;
    const std::string list(fl.begin(), fl.end()), want(suffix);
    size_t line = 0, at = 0;
    while (at < list.size()) {
        size_t e = list.find('\n', at); if (e == std::string::npos) e = list.size();
        std::string l = list.substr(at, e - at);
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
        for (char &c : l) c = (char)std::tolower((unsigned char)c);
        if (l.size() >= want.size() && l.compare(l.size() - want.size(), want.size(), want) == 0 && 12 * line + 12 <= fi.size()) {
            uint32_t size, off, comp;
            memcpy(&size, &fi[12 * line], 4); memcpy(&off, &fi[12 * line + 4], 4); memcpy(&comp, &fi[12 * line + 8], 4);
            if (comp == 0 && (size_t)off + size <= fs.size()) out.assign(fs.begin() + off, fs.begin() + off + size);
            else if (comp == 1 && (size_t)off + 4 <= fs.size()) {
                uint32_t clen; memcpy(&clen, &fs[off], 4);
                if ((size_t)off + 4 + clen <= fs.size()) out = lzss(std::vector<uint8_t>(fs.begin() + off + 4, fs.begin() + off + 4 + clen), size);
            }
            break;
        }
        at = e + 1; line++;
    }
    return out;
}

// Entry k of a u16-count, u16-offsets string table (areames.dc1, namedic.bin), still encoded.
std::string table_entry(const std::vector<uint8_t> &t, unsigned k) {
    uint16_t count = 0, o = 0;
    if (t.size() < 2) return "";
    memcpy(&count, t.data(), 2);
    if (k >= count || 2 + 2 * k + 2 > t.size()) return "";
    memcpy(&o, &t[2 + 2 * k], 2);
    std::string s;
    for (size_t x = o; x < t.size() && t[x]; x++) s += (char)t[x];
    return s;
}

const std::vector<std::string> &area_names() {
    static std::vector<std::string> names;
    static bool tried = false;
    if (tried) return names;
    tried = true;
    const std::vector<uint8_t> dc = archive_file("menu", "\\menu\\areames.dc1"), dic = archive_file("main", "\\namedic.bin");
    if (dc.size() < 2) return names;
    uint16_t count; memcpy(&count, dc.data(), 2);
    for (unsigned k = 0; k < count; k++) {
        const std::string raw = table_entry(dc, k);
        std::string enc;
        for (size_t x = 0; x < raw.size(); x++) {
            if (raw[x] == 0x0E && x + 1 < raw.size()) {   // a name from namedic.bin
                const unsigned c = (uint8_t)raw[++x];
                if (c >= 0x20) enc += table_entry(dic, c - 0x20);
                continue;
            }
            enc += raw[x];
        }
        std::string t = ff8_decode_text(enc.c_str());
        size_t p = t.find_first_not_of("- ");   // a name namedic.bin lacked leaves "- Hall"
        t = p == std::string::npos ? "" : t.substr(p);
        for (char &c : t) if (c == '"' || c == '\\') c = ' ';
        names.push_back(t);
    }
    return names;
}

void kv(std::string &j, const char *k, const std::string &v, bool comma = true) {
    j += '"'; j += k; j += "\":\""; j += v; j += '"'; if (comma) j += ',';
}

}  // namespace

std::string ff8_companion_snapshot() {
    savemap_ff8 *sm = ff8_externals.savemap;
    if (!sm) return "{\"ready\":0}";
    struct game_mode *gm = getmode_cached();
    const int mode = gm ? (int)gm->driver_mode : -1;
    static bool in_game = false;
    if (mode == MODE_FIELD || mode == MODE_BATTLE || mode == MODE_WORLDMAP) in_game = true;
    else if (mode == MODE_MAIN_MENU || mode == MODE_CREDITS || mode == MODE_GAMEOVER) in_game = false;
    if (!in_game) return "{\"ready\":0}";

    char loc[64] = "";
    uint32_t seconds = 0;
    ff7_ss_location(loc, sizeof loc, &seconds);
    seconds = sm->game_time;   // the live play clock (the header's played_time_secs is the last save's)
    if (mode == MODE_FIELD || mode == MODE_WORLDMAP || mode == MODE_MENU) {   // the place, as the save list names it
        const std::vector<std::string> &areas = area_names();
        const unsigned id = sm->header.location_id;
        if (id < areas.size() && !areas[id].empty()) snprintf(loc, sizeof loc, "%s", areas[id].c_str());
    }
    for (char *p = loc; *p; p++) if (*p == '"' || *p == '\\') *p = ' ';

    std::string j;
    j.reserve(8192);
    char b[256];
    snprintf(b, sizeof b, "{\"ready\":1,\"game\":\"ff8\",\"mode\":\"%s\",\"loc\":\"%s\",\"gil\":%u,\"time\":%u,\"speed2x\":%d,\"cine\":0,"
             "\"speed\":%d,\"noenc\":%d,\"bplus\":0,\"disc\":%u,",
             mode_name(mode), loc, sm->gil, seconds, gamehacks.companionSpeedhackEnabled() ? 1 : 0,
             gamehacks.companionSpeedhackEnabled() ? std::max(2, (int)gamehacks.getCurrentSpeedhack()) : 1,
             gamehacks.companionBattlesWanted() ? 0 : 1,
             (unsigned)*(const uint32_t *)&sm->header.curr_disk + 1);
    j += b;

    // the party: up to 3 members (0xFF ends the list)
    j += "\"party\":[";
    bool first = true;
    for (int slot = 0; slot < 3; slot++) {
        const unsigned id = sm->party[slot];
        if (id >= CHAR_NUM) continue;
        const savemap_ff8_character &c = sm->chars[id];
        const unsigned lv = std::min(100u, c.exp / 1000 + 1);
        unsigned hp = c.current_hp, mhp = std::max<uint16_t>(c.max_hp, c.current_hp);
        int atb = -1;
        if (mode == MODE_BATTLE && ff8_externals.battle_char_struct_dword_1D27B10) {   // the battle's own numbers
            const battle_slot s = slot_at(slot);
            if (s.com_id() != 0xFF && s.max_hp() > 0 && s.max_hp() < 100000) {
                hp = s.hp(); mhp = s.max_hp();
                if (s.max_atb() > 0) atb = (int)(std::min(s.atb(), s.max_atb()) * 65535ull / s.max_atb());
            }
        }
        snprintf(b, sizeof b, "%s{\"id\":%u,\"lv\":%u,\"hp\":%u,\"mhp\":%u,\"atb\":%d,", first ? "" : ",", id, lv, hp, mhp, atb);
        j += b;
        kv(j, "name", char_name(sm, id));
        // junctions: the magic on each stat
        static const char *const stat[] = { "HP", "Str", "Vit", "Mag", "Spr", "Spd", "Eva", "Hit", "Luck" };
        const uint8_t js[] = { c.j_hp, c.j_str, c.j_vit, c.j_mag, c.j_spr, c.j_spd, c.j_eva, c.j_hit, c.j_lck };
        j += "\"jx\":[";
        bool f2 = true;
        for (int s = 0; s < 9; s++) {
            if (!js[s]) continue;
            j += f2 ? "" : ",";
            j += "{\"s\":\""; j += stat[s]; j += "\",\"n\":\""; j += magic_name(js[s]); j += "\"}";
            f2 = false;
        }
        j += "],\"gf\":[";
        f2 = true;
        for (int g = 0; g < G_FORCE_NUM; g++) {
            if (!(c.gfs & (1u << g)) || !sm->gfs[g].exists) continue;
            j += f2 ? "\"" : ",\"";
            j += text((const char *)sm->gfs[g].name, 12);
            j += "\"";
            f2 = false;
        }
        // magic stock: 32 slots of {spell id, quantity}, as [name, count] (CompanionView drawList)
        j += "],\"mag\":[";
        f2 = true;
        for (int m = 0; m < 32; m++) {
            const unsigned sid = c.magics[m] & 0xFF, q = c.magics[m] >> 8;
            if (!sid || !q) continue;
            std::string n = magic_name(sid);
            if (n.empty()) continue;
            snprintf(b, sizeof b, "%s[\"%s\",%u]", f2 ? "" : ",", n.c_str(), q);
            j += b;
            f2 = false;
        }
        j += "]}";
        first = false;
    }
    j += "],";

    // the GFs the party has
    j += "\"gfs\":[";
    first = true;
    for (int g = 0; g < G_FORCE_NUM; g++) {
        const savemap_ff8_gf &gf = sm->gfs[g];
        if (!gf.exists) continue;
        snprintf(b, sizeof b, "%s{\"hp\":%u,\"exp\":%u,\"kills\":%u,", first ? "" : ",", (unsigned)gf.HPs, gf.exp, (unsigned)gf.kills);
        j += b;
        kv(j, "name", text((const char *)gf.name, 12), false);
        j += "}";
        first = false;
    }
    j += "],";

    // items, in the inventory's order
    j += "\"inv\":[";
    first = true;
    for (int i = 0; i < 198; i++) {
        const savemap_ff8_item &it = sm->items.items[i];
        if (!it.item_id || !it.item_quantity) continue;
        std::string n = item_name(it.item_id);
        if (n.empty()) continue;
        snprintf(b, sizeof b, "%s[\"%s\",%u]", first ? "" : ",", n.c_str(), (unsigned)it.item_quantity);
        j += b;
        first = false;
    }
    j += "],";

    // Triple Triad: the normal cards held (count in the low 7 bits), the rare ones (bitfield)
    {
        const savemap_ff8_triple_triad &tt = sm->triple_triad;
        unsigned kinds = 0, total = 0, rare = 0;
        j += "\"cards\":[";
        first = true;
        for (int i = 0; i < 77; i++) {
            const unsigned q = tt.cards[i] & 0x7F;
            if (!q) continue;
            kinds++; total += q;
            snprintf(b, sizeof b, "%s[\"%s\",%u]", first ? "" : ",", text(ff8_get_card_name(i), 24).c_str(), q);
            j += b;
            first = false;
        }
        for (int i = 0; i < 33; i++) if (tt.cards_rare[i / 8] & (1u << (i % 8))) {
            rare++;
            snprintf(b, sizeof b, "%s[\"%s\",1,1]", first ? "" : ",", text(ff8_get_card_name(77 + i), 24).c_str());
            j += b;
            first = false;
        }
        snprintf(b, sizeof b, "],\"tt\":{\"kinds\":%u,\"total\":%u,\"rare\":%u,\"wins\":%u,\"losses\":%u,\"draws\":%u},",
                 kinds, total, rare, (unsigned)tt.victory_count, (unsigned)tt.defeat_count, (unsigned)tt.equality_count);
        j += b;
    }
    // battle: the enemies in slots 3-7, with what Scan would show of them
    j += "\"enemies\":[";
    if (mode == MODE_BATTLE && ff8_externals.battle_char_struct_dword_1D27B10) {
        static const char *const elems[] = { "Fire", "Ice", "Thunder", "Earth", "Poison", "Wind", "Water", "Holy" };
        first = true;
        for (unsigned i = 3; i < 8; i++) {
            const battle_slot s = slot_at(i);
            if (s.com_id() == 0xFF || s.max_hp() == 0 || s.max_hp() > 10000000 || !s.info()) continue;
            if (s.status1() & 1) continue;                     // KO'd: gone from the battle
            // a slot the game is still filling can hold anything here (0x10202 was seen): not a pointer yet
            uint32_t name_va = 0;
            char name[25] = {};
            if (!safe_read(s.info(), &name_va, sizeof name_va) || !safe_read(name_va, name, sizeof name - 1)) continue;
            snprintf(b, sizeof b, "%s{\"slot\":%u,\"lv\":%u,\"hp\":%u,\"mhp\":%u,", first ? "" : ",", i - 3, (unsigned)s.level(),
                     s.hp(), s.max_hp());
            j += b;
            kv(j, "name", text(name, 24));
            std::string weak, absorb, nul, half;
            auto add = [](std::string &l, const char *n) { l += l.empty() ? "\"" : ",\""; l += n; l += "\""; };
            for (int e = 0; e < 8; e++) {
                const unsigned d = s.elem(e);
                if (d > 900) add(absorb, elems[e]);
                else if (d == 900) add(nul, elems[e]);
                else if (d < 800) add(weak, elems[e]);
                else if (d > 800) add(half, elems[e]);
            }
            std::string st;
            static const struct { int word; uint32_t bit; const char *n; } sts[] = {
                { 1, 1u << 2, "Petrify" }, { 1, 1u << 6, "Zombie" },
                { 2, 1u << 0, "Sleep" }, { 2, 1u << 1, "Haste" }, { 2, 1u << 2, "Slow" }, { 2, 1u << 3, "Stop" },
                { 2, 1u << 5, "Protect" }, { 2, 1u << 6, "Shell" }, { 2, 1u << 7, "Reflect" },
            };
            for (const auto &t : sts)
                if ((t.word == 1 ? s.status1() : s.status2()) & t.bit) add(st, t.n);
            j += "\"weak\":[" + weak + "],\"absorb\":[" + absorb + "],\"void\":[" + nul + "],\"half\":[" + half +
                 "],\"status\":[" + st + "]}";
            first = false;
        }
    }
    j += "],\"layout\":[]}";
    return j;
}
