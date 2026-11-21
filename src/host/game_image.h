/*
 * game_image.h - the game exe this build runs, in one place.
 *
 * FF8 Steam 2013 (FF8_EN.exe, app 39150), unwrapped by tools/unwrap_steamstub.py:
 *   image base 0x00400000, entry 0x0055ADB7 (mainCRTStartup)
 *   .text   0x00401000-0x00B69000
 *   .rdata  0x00B69000-0x00B6D000
 *   .data   0x00B6D000-0x0279F000 (0x00D1F000 on disk, the rest is .bss)
 *   .rsrc   0x0279F000-0x027C1000
 *   .dotemu 0x027C1000-0x027C2000
 *   .bind   0x027C2000-0x0282B000 (the SteamStub wrapper: mapped, never run)
 */
#ifndef GAME_IMAGE_H
#define GAME_IMAGE_H

#define GAME_EXE_NAME   "ff8_en.exe"
#define GAME_TEXT_LO    0x00401000u
#define GAME_TEXT_HI    0x00B69000u
#define GAME_IMAGE_LO   0x00401000u
#define GAME_IMAGE_HI   0x0282B000u
#define GAME_DATA_LO    0x00B6D000u   /* .data and .bss */
#define GAME_DATA_HI    0x0279F000u

#endif
