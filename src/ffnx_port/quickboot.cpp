// quickboot.cpp - straight from launch to the game: skips the credits, picks New Game (or
// Continue and the first save) on the title screen, and skips the opening FMV, by driving the
// game's own flow -- the presses a player would make, timed off the game's mode instead of
// fixed delays. Off unless asked for:
//   files/quickboot.txt  (the launcher's Start setting)  "newgame", "continue" or "resume"
//   FF7_QUICKBOOT=newgame|continue|resume                (env.txt, tests)
// "resume [N]" loads save-state slot N (default: the current slot, savestates/current.txt;
// src/host/ff8_savestate.c) once a game is running: it continues the first save (or starts a
// new game when there is none) and loads the state in the field. A state cannot be loaded over
// the title screen or the credits (#21: it crashed there).
// Ticked once per frame from ffnx_recomp::step_code (ff7_limit_fps). Stops for good once the
// field is reached, after 40 s, or at the first real press of a button.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include "common.h"
#include "globals.h"
#include "log.h"
#include "ff7/defs.h"
#include "ff8.h"
#include "ff8_savestate.h"

extern "C" void ff7_platform_pad_inject(unsigned short buttons);   // src/android (the pad the game reads)
extern "C" int ff7_platform_pad_buttons(void);                    // the player's own buttons

namespace {

constexpr unsigned short PAD_UP = 0x0001, PAD_START = 0x0010, PAD_OK = 0x2000;   // XInput (OK = Circle = B)
constexpr uint32_t TITLE_CURSOR = 0x00DD6F24u;   // title menu: 0 New Game, 1 Continue (found with FF7_MEMDIFF)

int g_target = -2;            // -2 not configured yet, -1 off, 0 new game, 1 continue, 2 resume a state
bool g_done;
uint32_t g_mode = ~0u;
int64_t g_mode_since, g_started, g_last_press, g_last_skip;
int g_title_presses;
char g_resume_slot[48] = "current";
int g_flow = -1;              // the title menu's choice: 0 new game, 1 continue (resume goes through one)
int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Any save file in the game folder (the process's working directory)? FF7: save/saveNN.ff7;
// FF8: save/user_0/slotN_saveNN.ff8 (FFNx's userdata path, get_userdata_path).
bool have_saves() {
    const char *dir = ff8 ? "save/user_0" : "save";
    const char *prefix = ff8 ? "slot" : "save", *ext = ff8 ? ".ff8" : ".ff7";
    DIR *d = opendir(dir);
    if (!d) return false;
    bool any = false;
    while (dirent *e = readdir(d)) {
        size_t n = std::strlen(e->d_name);
        if (n > 4 && !strncasecmp(e->d_name, prefix, 4) && !strcasecmp(e->d_name + n - 4, ext)) { any = true; break; }
    }
    closedir(d);
    return any;
}

// Save-state slot `slot` ("3", or "current": savestates/current.txt) exists?
bool have_state(const char *slot) {
    char cwd[1024], path[1100];
    if (!getcwd(cwd, sizeof cwd)) return false;
    int k = std::atoi(slot);
    if (!k && std::strcmp(slot, "current")) {                 // a named state (tests)
        std::snprintf(path, sizeof path, "%s/../savestates/%s.ff7s", cwd, slot);
        return access(path, R_OK) == 0;
    }
    if (!k) {
        std::snprintf(path, sizeof path, "%s/../savestates/current.txt", cwd);
        k = 1;
        if (std::FILE *f = (std::fopen)(path, "rb")) { if (std::fscanf(f, "%d", &k) != 1) k = 1; std::fclose(f); }
        std::snprintf(path, sizeof path, "%s/../savestates/quick.ff7s", cwd);
        if (k == 1 && access(path, R_OK) == 0) return true;   // not migrated to slot 1 yet
    }
    std::snprintf(path, sizeof path, "%s/../savestates/slot%02d.ff7s", cwd, k);
    return access(path, R_OK) == 0;
}

int read_target() {
    const char *e = std::getenv("FF7_QUICKBOOT");
    char buf[96] = "";
    if (e) std::snprintf(buf, sizeof buf, "%s", e);
    else {
        // files/quickboot.txt, beside the game folder the process runs in; a host path, so
        // around FFNx's fopen (which maps game paths)
        char cwd[1024], path[1100];
        if (getcwd(cwd, sizeof cwd)) {
            std::snprintf(path, sizeof path, "%s/../quickboot.txt", cwd);
            if (std::FILE *f = (std::fopen)(path, "rb")) {
                size_t n = std::fread(buf, 1, sizeof buf - 1, f);
                buf[n] = 0;
                std::fclose(f);
            }
        }
    }
    if (!std::strncmp(buf, "newgame", 7)) return 0;
    if (!std::strncmp(buf, "resume", 6)) {
        // "resume N" (slot N), or "resume NAME": a named state, savestates/NAME.ff7s (tests)
        const char *arg = buf + 6;
        while (*arg == ' ') arg++;
        char name[sizeof g_resume_slot] = "";
        for (size_t i = 0; arg[i] && i + 1 < sizeof name && (std::isalnum((unsigned char)arg[i]) || arg[i] == '-' || arg[i] == '_'); i++) {
            name[i] = arg[i];
            name[i + 1] = 0;
        }
        if (name[0]) std::snprintf(g_resume_slot, sizeof g_resume_slot, "%s", name);
        if (have_state(g_resume_slot)) return 2;
        ffnx_info("quickboot: no save state yet, starting a new game instead\n");
        return 0;
    }
    if (!std::strncmp(buf, "continue", 8)) {
        if (have_saves()) return 1;
        ffnx_info("quickboot: no save files yet, starting a new game instead\n");
        return 0;
    }
    return -1;
}

void finish(const char *why) {
    g_done = true;
    ff7_platform_pad_inject(0);
    ffnx_info("quickboot: %s after %.1f s\n", why, (now_ms() - g_started) / 1000.0);
}

// One press: held ~80 ms, then released, at most every `gap` ms.
void press(unsigned short b, int64_t t, int gap) {
    if (t - g_last_press >= gap) { ff7_platform_pad_inject(b); g_last_press = t; }
    else if (t - g_last_press >= (ff8 ? 250 : 80)) ff7_platform_pad_inject(0);   // FF8 polls less often (15 FPS screens)
}

}  // namespace

namespace ffnx_recomp {

void quickboot_frame() {
    if (g_target == -2) {
        g_target = read_target();
        g_started = now_ms();
        if (g_target >= 0) ffnx_info("quickboot: %s\n", g_target == 2 ? "resume the save state" : g_target ? "continue" : "new game");
        g_flow = g_target == 2 ? (have_saves() ? 1 : 0) : g_target;
    }
    if (g_target < 0 || g_done) return;
    const int64_t t = now_ms();
    if (t - g_started > (ff8 ? 120000 : 40000)) { finish("gave up"); return; }   // FF8: its logo comes later
    if (ff7_platform_pad_buttons()) { finish("stopped by a button press"); return; }

    struct game_mode *m = getmode_cached();
    const uint32_t mode = m ? m->driver_mode : ~0u;
    if (mode != g_mode) { g_mode = mode; g_mode_since = t; ff7_platform_pad_inject(0); ffnx_info("quickboot: mode %d\n", (int)mode); }
    const int64_t in_mode = t - g_mode_since;

    switch (mode) {
    case MODE_CREDITS:
        if (ff8) { if (in_mode > 1000) press(0x1000, t, 1500); }   // FF8: the Squaresoft logo, confirm (XInput B)
        else if (in_mode > 300) press(PAD_START, t, 250);
        break;
    case MODE_MAIN_MENU: {
        if (in_mode < 800) break;                            // the menu fading in
        if (ff8) {
            // FF8: a state loads only where the game runs at the depth it was saved at (the
            // field, not the title: "host stack differs"), so "resume" and "newgame" both start
            // a new game: the cursor comes up on Continue, Up is NEW GAME, then confirm (XInput
            // B). The opening movie is skipped in the field below. "continue" is left to the
            // player (FF7's title cursor address does not apply).
            if (g_target == 1) { finish("FF8: Continue is the player's choice"); break; }
            if (in_mode < 1500) break;
            if (g_title_presses == 0) { press(PAD_UP, t, 0); g_title_presses = 1; }
            else if (g_title_presses == 1 && t - g_last_press > 700) { press(0x1000, t, 0); g_title_presses = 2; }
            else if (t - g_last_press >= 250) ff7_platform_pad_inject(0);
            break;
        }
        const uint8_t cursor = *(volatile uint8_t *)(uintptr_t)TITLE_CURSOR;
        if (g_title_presses == 0) {
            if (cursor != g_flow) press(PAD_UP, t, 300);
            else if (t - g_last_press >= 300) { press(PAD_OK, t, 0); g_title_presses = 1; }
            else ff7_platform_pad_inject(0);
        } else if (g_flow == 1) {
            press(PAD_OK, t, 600);                           // Continue: the save file, then its first slot
        } else {
            ff7_platform_pad_inject(0);
        }
        break;
    }
    case MODE_FIELD:
        ff7_platform_pad_inject(0);
        if (ff8 ? (ff8_externals.movie_object && ff8_externals.movie_object->movie_is_playing)
                : (ff7_externals.movie_object && ff7_externals.movie_object->is_playing)) {
            if (t - g_last_skip > 500) { if (ff8) ff8_skip_movies(); else ff7_skip_movies(); g_last_skip = t; }
        } else if ((g_last_skip && t - g_last_skip > 1500) || in_mode > 6000) {
            if (g_target == 2 && ff8) {
                // FF8: the field loop must be back at the depth the state was saved at; just
                // after the opening movie it is not yet ("host stack differs"). Ask, and ask
                // again 2 s after a refusal, until it loads (or the time runs out).
                static int64_t asked;
                const int r = asked ? ff7_savestate_load_result() : -1;
                if (r == 1) finish("in the field, the save state loaded");
                else if (r == -1 && t - asked > 2000) { ff7_savestate_request(1, g_resume_slot); asked = t; }
            } else if (g_target == 2) {                      // resume: a game is running now
                ff7_savestate_request(1, g_resume_slot);
                finish("in the field, loading the save state");
            } else {
                finish("in the field");                      // the FMV starts a moment into the field
            }
        }
        break;
    default:
        // FF8's Squaresoft logo waits for a button before the title menu: confirm (XInput B)
        if (ff8 && in_mode > 1000) press(0x1000, t, 1500);
        else ff7_platform_pad_inject(0);
        break;
    }
}

}  // namespace ffnx_recomp
