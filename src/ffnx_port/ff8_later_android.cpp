// ff8_later_android.cpp - vanilla stand-ins for the FFNx FF8 files not ported yet
// (cmake/ffnx.cmake FF8_LATER): the kernel.bin extensions (magic, ability, command, item), the
// extended battle files (monsters), the in-menu volume sliders, FFNx's own FF8 rumble and its
// Remastered-assets mode. Each answers the way FFNx behaves when the extension has nothing to
// do (no extended kernel mod, no Remastered install): the game keeps its own tables and code.
//
// When a file is ported it leaves FF8_LATER and its stand-ins here are deleted.
#include <cstdint>
#include <span>
#include <unordered_map>

#include "ff8.h"
#include "globals.h"
#include "ff8/kernel_item.h"
#include "ff8/kernel_magic.h"
#include "ff8/kernel_ability.h"
#include "ff8/remaster.h"
#include "ff8/vibration.h"
#include "ff8/battle/monsters.h"
#include "ff8/menu/volume.h"

// kernel_item.cpp: inactive unless a mod grows the item table; the inventory is the savemap's.
bool ff8_kernel_item_active() { return false; }
std::span<savemap_ff8_item> ff8_player_items() { return ff8_externals.savemap->items.items; }
int ff8_kernel_item_add(int, int) { return 0; }   // only called when active

// kernel_magic.cpp / kernel_ability.cpp: the game's own kernel.bin handling.
void ff8_kernel_magic_init() {}
void ff8_kernel_ability_init() {}
// Voice acting for battle command names (voice.cpp): no name, so no voice line.
const char *ff8_get_battle_command_name(int) { return nullptr; }

// battle/monsters.cpp: the game's own battle file list and scan camera rows.
void ff8_battle_monsters_init() {}
const uint8_t *ff8_get_scan_camera_rows() { return nullptr; }
void ff8_set_scan_camera_rows(const uint8_t *) {}

// menu/volume.cpp: the game's own sound menu.
void ff8_menu_volume_init() {}

// vibration.cpp: FFNx's FF8 rumble (the app has its own, src/ffnx_port/rumble.cpp).
void vibration_init() {}

// remaster.cpp: FFNx's mode for the 2000 exe with Remastered assets (FFVIII.exe beside it),
// never the case here.
void ff8_remaster_init() {}
void ff8_remaster_set_field_model_scaling(const std::unordered_map<uint32_t, CharaOneModel> &) {}
