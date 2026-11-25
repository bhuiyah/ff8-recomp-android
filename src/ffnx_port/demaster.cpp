// demaster.cpp - textures of Demaster mods (the Remastered's mod loader on Windows) that stand for a
// whole texture page. Demaster reads, under DEMASTER_EXP/textures:
//   field_bg/<xx>/<map>/<map>_<page>[_<palette>]        field backgrounds (pages from VRAM page 16 on)
//   battle.fs/hd_new/a0stg<nnn>_<VRAM page>[_<frame>]   battle stages (pages 16 to 21)
//   world/dat/wmset/wmset_<VRAM page>_<palette>         the world map's pages 13, 14, 28 and 29
//   world/dat/texl/texl_<nnn>_0                         the world map's big textures (mods.cpp names
//                                                       them as FFNx's world/dat/texl/texture<nnn - 1>)
// each as .dds, else .png. FFNx names the same pages its own way; here its name and the VRAM page
// (both kept in the texture's name buffer, tools/ffnx_edit_ff8.py) become Demaster's.
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include "include/demaster.h"

extern "C" const char *ff7_mods_redirect(const char *rel, char *buf, unsigned n);   // src/mods/mods.cpp

namespace {
bool starts(const char *s, const char *with) {
    for (; *with; s++, with++) if (tolower((unsigned char)*s) != *with) return false;
    return true;
}

// textures/<rel>.dds or .png of an active mod, with or without DEMASTER_EXP/ above.
const char *find(const std::string &rel, char *buf, unsigned n) {
    for (const char *ext : { ".dds", ".png" })
        for (const char *top : { "demaster_exp/textures/", "textures/" })
            if (const char *p = ff7_mods_redirect((top + rel + ext).c_str(), buf, n)) return p;
    return nullptr;
}
}  // namespace

extern "C" const char *ff8_demaster_page(const char *name, int page, unsigned palette, int *paletted, char *buf, unsigned n) {
    *paletted = 0;
    if (!name || page < 0) return nullptr;
    const std::string pal = "_" + std::to_string(palette);
    if (starts(name, "field/mapdata/")) {
        // field/mapdata/bc/bcgate_1/bcgate_1_0: the page comes from the VRAM position (the name's number
        // may be the old Tonberry one)
        const std::string map = name + 14;
        const size_t a = map.find('/'), b = map.rfind('/'), u = map.rfind('_');
        if (a != 2 || b == a || u == std::string::npos || u < b || page < 16) return nullptr;
        const std::string rel = "field_bg/" + map.substr(0, u) + "_" + std::to_string(page - 16);
        if (const char *p = find(rel + pal, buf, n)) { *paletted = 1; return p; }
        return find(rel, buf, n);
    }
    if (starts(name, "battle/a0stg")) {
        const char *d = name + 12;
        if (!isdigit((unsigned char)d[0]) || !isdigit((unsigned char)d[1]) || !isdigit((unsigned char)d[2]) || page < 16 || page > 21) return nullptr;
        const std::string rel = "battle.fs/hd_new/a0stg" + std::string(d, 3) + "_" + std::to_string(page);
        if (const char *p = find(rel + "_0", buf, n)) return p;   // the first frame of an animated one
        return find(rel, buf, n);
    }
    if (starts(name, "world/dat/") && (page == 13 || page == 14 || page == 28 || page == 29)) {
        char no[8];
        snprintf(no, sizeof no, "%03d", page);
        const std::string rel = std::string("world/dat/wmset/wmset_") + no;
        if (palette != 0)
            if (const char *p = find(rel + pal, buf, n)) { *paletted = 1; return p; }
        return find(rel + "_0", buf, n);
    }
    return nullptr;
}

extern "C" const char *ff8_demaster_dds(const char *archived, char *buf, unsigned n) {
    if (!starts(archived, "textures\\") && !starts(archived, "textures/")) return nullptr;
    std::string rel = archived;
    for (char &c : rel) c = c == '\\' ? '/' : (char)tolower((unsigned char)c);
    const size_t dot = rel.rfind('.');
    if (dot == std::string::npos || rel.compare(dot, std::string::npos, ".png") != 0) return nullptr;
    rel.replace(dot, 4, ".dds");
    if (const char *p = ff7_mods_redirect(rel.c_str(), buf, n)) return p;
    return ff7_mods_redirect(("demaster_exp/" + rel).c_str(), buf, n);
}
