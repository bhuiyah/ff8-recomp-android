// companion_ff8.cpp - FF8's live game state for the bottom screen (CompanionView), the FF8 half of
// companion.cpp's snapshot: the party (HP, level, junctions, magic stock), the GFs, items and
// Triple Triad cards, gil, play time and where the party is. Read from the game's own savemap
// (FFNx ff8/save_data.h, the game keeps it live) and its kernel.bin text (ff8/kernel.h).
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
    if (id <= 32) return text(ktext(ff8_kernel_section::battle_item_text, k->battle_items[id - 1].offset_item_name));
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
battle_slot slot_at(unsigned i) {
    return { (const uint8_t *)ff8_externals.battle_char_struct_dword_1D27B10 + i * SLOT_SIZE };
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
            snprintf(b, sizeof b, "%s{\"slot\":%u,\"lv\":%u,\"hp\":%u,\"mhp\":%u,", first ? "" : ",", i - 3, (unsigned)s.level(),
                     s.hp(), s.max_hp());
            j += b;
            kv(j, "name", text(guest::ptr_of<char>(*guest::ptr_of<uint32_t>(s.info())), 24));
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
