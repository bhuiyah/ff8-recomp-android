/*
 * ff7_af3dn.c - the AF3DN.P imports, until FFNx itself is linked as the driver.
 *
 * The 2013 DotEmu 1.02 exe reads its configuration through registry functions exported
 * by the graphics driver (dotemuReg*). These are a C port of FFNx's own implementations
 * (julianxhokaxhiu/FFNx src/common.cpp, GPL-3.0), so the game is configured exactly as it
 * will be under FFNx: custom driver (3) at AF3DN.P, mode 2, full install, paths at the
 * install root. Install paths use FF7_GUEST_ROOT, which ff7_paths.c maps to the host
 * game folder.
 *
 * new_dll_graphics_driver is the renderer itself (FFNx). It stops the run by name.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "win32hle.h"
#include "ff7_hle.h"
#include "guest_alloc.h"

#define ERROR_SUCCESS 0u
#define W8(p)  (*(uint8_t *)(uintptr_t)(p))
#define W32(p) (*(uint32_t *)(uintptr_t)(p))

/* The game's volumes are FFNx's external_sfx_volume / external_music_volume (FFNx.toml, which
 * the launcher's Settings > Sound writes), as under FFNx on Windows. -1 there means "unset":
 * FFNx would read the Steam ff7sound.cfg, which Android does not have, so it is 100 here.
 * Weak, for builds without FFNx linked. */
extern long external_sfx_volume __attribute__((weak));
extern long external_music_volume __attribute__((weak));
static long g_sfx_volume = -1, g_music_volume = -1;   /* the game's own Config menu, this session */

static long *sfx_cfg(void)   { return &external_sfx_volume ? &external_sfx_volume : &g_sfx_volume; }
static long *music_cfg(void) { return &external_music_volume ? &external_music_volume : &g_music_volume; }
static uint32_t volume_of(long v) { return v < 0 ? 100u : v > 100 ? 100u : (uint32_t)v; }

static void d_RegOpenKeyExA(void)  { RET(ERROR_SUCCESS, 5); }   /* (hKey, sub, opts, sam, phk) */
static void d_RegCloseKey(void)    { RET(ERROR_SUCCESS, 1); }
static void d_RegFlushKey(void)    { RET(ERROR_SUCCESS, 1); }
static void d_RegDeleteValueA(void){ RET(ERROR_SUCCESS, 2); }

/* (hKey, name, reserved, type, data, cb) */
static void d_RegSetValueExA(void) {
    const char *name = ASTR(1);
    uint32_t data = A32(4);
    if (name && data) {
        if (!strcmp(name, "SFXVolume")) {
            if (W8(data) > 0x64) W8(data) = 0x64;
            *sfx_cfg() = W8(data);
        } else if (!strcmp(name, "MusicVolume")) {
            if (W8(data) > 0x64) W8(data) = 0x64;
            *music_cfg() = W8(data);
        }
    }
    RET(ERROR_SUCCESS, 6);
}

/* (hKey, name, reserved, type*, data, cb*) */
static void d_RegQueryValueExA(void) {
    const char *name = ASTR(1);
    uint32_t data = A32(4), pcb = A32(5);
    uint32_t cb = pcb ? W32(pcb) : 0;
    char *out = (char *)(uintptr_t)data;
    uint32_t ret = ERROR_SUCCESS;
    if (getenv("FF7_TRACE_REG")) fprintf(stderr, "[reg] query %s (cb=%u)\n", name ? name : "(null)", cb);
    if (!name || !data) RET(ERROR_SUCCESS, 6);

    if (!strcmp(name, "AppPath"))        snprintf(out, cb, "%s\\", FF7_GUEST_ROOT);
    else if (!strcmp(name, "DataPath"))  snprintf(out, cb, "%s\\data\\", FF7_GUEST_ROOT);
    else if (!strcmp(name, "DataDrive")) snprintf(out, cb, "CD:");
    else if (!strcmp(name, "MoviePath")) snprintf(out, cb, "%s\\data\\movies\\", FF7_GUEST_ROOT);
    else if (!strcmp(name, "FullInstall")) W8(data) = 1;
    else if (!strcmp(name, "DD_GUID") || !strcmp(name, "Sound_GUID")) memset(out, 0, cb);
    else if (!strcmp(name, "MIDI_DeviceID")) W8(data) = 0;
    else if (!strcmp(name, "Sound") || !strcmp(name, "Midi") || !strcmp(name, "wave_music")) ret = 2;
    /* The whole DWORD: the exe (0x4075B0) reads into an uninitialised stack DWORD and clamps it
     * to 0..100, so a lone low byte (0xFF for "unset") left the volume to stack garbage: 0 or 100,
     * whatever the launcher said (GitHub #55). */
    else if (!strcmp(name, "SFXVolume"))   { if (cb >= 4) W32(data) = volume_of(*sfx_cfg()); else W8(data) = (uint8_t)volume_of(*sfx_cfg()); }
    else if (!strcmp(name, "MusicVolume")) { if (cb >= 4) W32(data) = volume_of(*music_cfg()); else W8(data) = (uint8_t)volume_of(*music_cfg()); }
    else if (!strcmp(name, "Driver"))      W8(data) = 3;
    else if (!strcmp(name, "DriverPath"))  snprintf(out, cb, "AF3DN.P");
    else if (!strcmp(name, "Mode")) {
        W8(data) = 2;
        if (cb == 256) ret = 1;          /* FFNx: the Steam build asks repeatedly; 1 -> 2 internally */
    }
    else if (!strcmp(name, "Options"))     W8(data) = 0;
    /* FF8 (FFNx's dotemuRegQueryValueExA, FF8 branch). FFNx sets only the low byte of the DWORD
     * values; the whole DWORD is written here so no stack garbage rides along (see #55 above). */
    else if (!strcmp(name, "MIDIGUID")) {
        static const uint32_t microsoft_synthesizer[4] = { 0x58C2B4D0, 0x11D146E7, 0xA000AC89, 0x294105C9 };
        memcpy(out, microsoft_synthesizer, 16);
    }
    else if (!strcmp(name, "GraphicsGUID") || !strcmp(name, "SoundGUID")) memset(out, 0, 16);
    else if (!strcmp(name, "SoundOptions"))   { if (cb >= 4) W32(data) = 0x00000000; else W8(data) = 0x00; }
    else if (!strcmp(name, "InstallOptions")) { if (cb >= 4) W32(data) = 0x000000FF; else W8(data) = 0xFF; }
    else if (!strcmp(name, "MidiOptions"))    { if (cb >= 4) W32(data) = 0x00000001; else W8(data) = 0x01; }
    else if (!strcmp(name, "Graphics"))       { if (cb >= 4) W32(data) = 0x00100021; else W8(data) = 0x21; }
    else fprintf(stderr, "[reg] unknown value %s, answered as empty\n", name);
    RET(ret, 6);
}

/* ---- FF8's file functions from AF3DN.P (FFNx common.cpp dotemuCreateFileA & co) ---------- */

/* A kernel32 shim's function by name, to run it on the current guest frame (same arguments). */
extern const win32hle_shim win32hle_kernel32_ext[], win32hle_kernel32_crt[], ff7_kernel32[];
static recomp_func_t shim_fn(const char *name) {
    const win32hle_shim *tables[] = { ff7_kernel32, win32hle_kernel32_ext, win32hle_kernel32_crt };
    for (unsigned t = 0; t < sizeof tables / sizeof *tables; t++)
        for (const win32hle_shim *s = tables[t]; s->name; s++)
            if (!strcmp(s->name, name)) return s->fn;
    hle_fatal("ff7_af3dn: no %s shim to forward to", name);
    return NULL;
}

/* FFNx's redirection (src/ffnx_port/ffnx_boot.cpp); weak: the headless runner has no FFNx. */
int ffnx_ff8_steam_redirection(const char *name, char *out) __attribute__((weak));

/* (name, access, share, sa, disposition, flags, template): FFNx's Steam redirection first
 * (CD:\...\DISKn -> <app path>\data\disk\DISKn), then CreateFileA itself. */
static void d_CreateFileA(void) {
    static char *redirected;   /* guest buffer: the path the game's call now names */
    const char *name = ASTR(0);
    char path[260];
    if (name && ffnx_ff8_steam_redirection && ffnx_ff8_steam_redirection(name, path)) {
        if (!redirected) redirected = (char *)guest_malloc(260);
        /* FFNx's user-data paths are host paths under the game folder (get_userdata_path):
         * the game names them under its install root, which ff7_paths.c maps back. */
        char cwd[512];
        size_t cl = getcwd(cwd, sizeof cwd) ? strlen(cwd) : 0;
        if (cl && !strncmp(path, cwd, cl) && (path[cl] == '/' || path[cl] == 0)) {
            snprintf(redirected, 260, "%s%s", FF7_GUEST_ROOT, path + cl);
            for (char *c = redirected; *c; c++) if (*c == '/') *c = '\\';
        } else {
            snprintf(redirected, 260, "%s", path);
        }
        if (getenv("FF7_PATH_TRACE")) fprintf(stderr, "[dotemu] CreateFileA %s -> %s\n", name, redirected);
        MEM32(g_esp + 4) = (uint32_t)(uintptr_t)redirected;   /* argument 0 */
    }
    shim_fn("CreateFileA")();
}

/* "CD:" is the game's disc: a CD-ROM drive (DRIVE_CDROM 5), as FFNx answers. */
static void d_GetDriveTypeA(void) {
    const char *root = ASTR(0);
    if (root && !strcmp(root, "CD:")) RET(5u, 1);
    shim_fn("GetDriveTypeA")();
}

static void d_DeleteFileA(void) { shim_fn("DeleteFileA")(); }

/* new_dll_graphics_driver is the renderer itself: src/ffnx_bridge/ffnx_driver.cpp exports
 * it (registered by ff7_hle_register before this table). */

const win32hle_shim ff7_af3dn[] = {
    { "dotemuRegOpenKeyExA", d_RegOpenKeyExA },
    { "dotemuRegCloseKey", d_RegCloseKey },
    { "dotemuRegFlushKey", d_RegFlushKey },
    { "dotemuRegDeleteValueA", d_RegDeleteValueA },
    { "dotemuRegSetValueExA", d_RegSetValueExA },
    { "dotemuRegQueryValueExA", d_RegQueryValueExA },
    { "dotemuCreateFileA", d_CreateFileA },
    { "dotemuGetDriveTypeA", d_GetDriveTypeA },
    { "dotemuDeleteFileA", d_DeleteFileA },
    { NULL, NULL }
};
