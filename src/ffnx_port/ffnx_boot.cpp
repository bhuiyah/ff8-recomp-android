// ffnx_boot.cpp - FFNx's process attach, in a build that links FFNx (cmake/ffnx.cmake).
//
// On Windows FFNx is AF3DN.P and its DllMain runs before the game's WinMain: it detects the
// exe version from code bytes at 0x401004, fills the address tables (externals_102_us.h),
// reads FFNx.toml and installs its hooks (replace_function → recomp entry overrides here,
// src/ffnx_port/patch_recomp.cpp). Here the same DllMain runs from ff8_boot.c once the
// image is mapped and the guest heap is up, before the lifted entry is called.
//
// Both hooks are strong definitions of weak symbols in ff7_core, so the runtime without FFNx
// (the bridge self-test build) keeps working:
//   ffnx_bridge_register  exports FFNx's own new_dll_graphics_driver to the game (the
//                         bridge's stub driver in src/ffnx_bridge/ffnx_driver.cpp is unused)
//   ff7_ffnx_attach       calls DllMain(DLL_PROCESS_ATTACH)
#include <cstdio>
#include "guest_abi.h"
#include "ffnx_android.h"

extern "C" void *new_dll_graphics_driver(void *game_object);               // FFNx common.cpp
extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved); // FFNx common.cpp
extern "C" void ffnx_recomp_report(void);                                   // patch_recomp.cpp

extern "C" void ffnx_bridge_register(void) {
    guest::guest_export<&new_dll_graphics_driver>("new_dll_graphics_driver");
}

void framegen_init();                                                       // framegen.cpp
extern "C" int ff7_ffnx_attach(void) {
    std::fprintf(stderr, "[ffnx] DllMain(DLL_PROCESS_ATTACH)\n");
    framegen_init();   // before the renderer starts
    BOOL ok = DllMain(FFNX_MODULE, DLL_PROCESS_ATTACH, nullptr);
    ffnx_recomp_report();
    std::fprintf(stderr, "[ffnx] DllMain returned %d\n", (int)ok);
    return ok ? 0 : 1;
}

// FF8: FFNx's path redirection for the Steam 2013 exe's file opens (dotemuCreateFileA:
// "CD:\FF8\DATA\DISK1" -> <app path>\data\disk\DISK1, ...), for src/hle/ff8_af3dn.c (C).
bool ff8_steam_redirection(const char *lpFileName, char *newPath, bool *isZzzFile);   // FFNx ff8/file.cpp
bool ff8_fs_last_fopen_is_redirected();
extern uint32_t ff8_remastered_edition;                                             // FFNx globals.h
extern "C" int ffnx_ff8_steam_redirection(const char *name, char *out) {
    // The Remastered edition runs the 2000 exe, which opens files with CreateFileA itself
    // (no Dotemu wrapper): FFNx's own hooks read main.zzz / other.zzz, nothing to redirect here
    if (ff8_remastered_edition) return 0;
    if (ff8_fs_last_fopen_is_redirected()) return 0;
    out[0] = 0;
    return ff8_steam_redirection(name, out, nullptr) ? 1 : 0;
}
