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
#define GAME_IMAGE_HI   GAME_TAIL_PAGE_END
#define GAME_DATA_LO    0x00B6D000u   /* .data and .bss */
#define GAME_DATA_HI    GAME_TAIL_PAGE_END

/* The program's memory from GAME_TAIL_CUT up lives GAME_TAIL_DELTA higher: Android 16 has the Java
 * heap at 0x02000000 (GitHub #2; ff8_tail.c, tools/tail_reloc.py). */
#define GAME_TAIL_CUT      0x01F9DC40u
#define GAME_TAIL_PAGE_END 0x01F9E000u   /* the page the cut is in stays mapped in place too */
#define GAME_DATA_OLD_HI   0x0279F000u
#define GAME_TAIL_OLD_END  0x0282B000u   /* end of the image as linked */
#define GAME_TAIL_DELTA    0x69180000u   /* low 16 bits 0: a mod's 16-bit write into an address still fits */
#define GAME_TAIL_LO       (0x01F9D000u + GAME_TAIL_DELTA)                  /* 0x6B11D000 */
#define GAME_TAIL_HI       (GAME_TAIL_OLD_END + GAME_TAIL_DELTA)            /* 0x6B9AB000 */
#define GAME_TAIL_DATA_HI  (GAME_DATA_OLD_HI + GAME_TAIL_DELTA)

#endif
