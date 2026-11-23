/*
 * ff8_tail.c - the game's memory from GAME_TAIL_CUT up lives GAME_TAIL_DELTA higher.
 *
 * FF8's program spans 0x00400000-0x0279F000 and has no relocations. From Android 16, ART's Java
 * heap starts at 0x02000000: mapping the program over it wiped live Java objects and the app
 * died as the game started (GitHub #2). The part above GAME_TAIL_CUT is zero-filled .bss; it is
 * moved, and config/tail_reloc.txt (tools/tail_reloc.py) lists every place in the program that
 * holds one of its addresses. The lift applies the same table, so translated code has the new
 * addresses in it; here the program in memory gets them (FFNx and the mods read its code).
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "game_image.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static const uint8_t k_table[] = {
#include "../../config/tail_reloc.txt"
};

/* [lo, hi) taken, as /proc/self/maps has it, for the log */
static void print_taken(uint32_t lo, uint32_t hi) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        unsigned long long a, b;
        if (sscanf(line, "%llx-%llx", &a, &b) == 2 && a < hi && b > lo) fprintf(stderr, "[tail]   %s", line);
    }
    fclose(f);
}

static int reserve(uint32_t lo, uint32_t hi) {
    void *p = mmap((void *)(uintptr_t)lo, hi - lo, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == (void *)(uintptr_t)lo) return 1;
    if (p != MAP_FAILED) munmap(p, hi - lo);   /* a kernel without MAP_FIXED_NOREPLACE */
    return 0;
}

static int g_image_ok, g_tail_ok;

/* As the library loads, before the app's threads map more. */
__attribute__((constructor(101))) static void reserve_early(void) {
    g_image_ok = reserve(0x00400000u, GAME_TAIL_PAGE_END);
    g_tail_ok = reserve(GAME_TAIL_LO, GAME_TAIL_HI);
}

/* pe_loader.c: how much of the image at `base` is mapped in place; 0 = all of it. */
uint32_t recomp_pe_low_end(uint32_t base, uint32_t span) {
    (void)span;
    return base == 0x00400000u ? GAME_TAIL_PAGE_END : 0;
}

/* pe_loader.c: where the image's byte at `va` is in memory. */
uint32_t recomp_pe_host_va(uint32_t va) { return va >= GAME_TAIL_CUT && va < GAME_TAIL_OLD_END ? va + GAME_TAIL_DELTA : va; }

/* pe_loader.c, before anything is mapped. */
int recomp_pe_reserve(void) {
    if (!g_image_ok) {
        fprintf(stderr, "[tail] something is already mapped in the game's range 0x00400000-0x%08X (errno %d):\n", GAME_TAIL_PAGE_END, errno);
        print_taken(0x00400000u, GAME_TAIL_PAGE_END);
    }
    if (!g_tail_ok && !reserve(GAME_TAIL_LO, GAME_TAIL_HI)) {
        fprintf(stderr, "[tail] FATAL: 0x%08X-0x%08X is taken on this device:\n", GAME_TAIL_LO, GAME_TAIL_HI);
        print_taken(GAME_TAIL_LO, GAME_TAIL_HI);
        return -1;
    }
    /* Test: FF8_OLD_TAIL_GUARD=1 makes 0x02000000 up to the usual Java heap fault on any touch:
     * the memory Android 16's heap has there, and where a missed address would point. */
    if (getenv("FF8_OLD_TAIL_GUARD")) {
        void *p = mmap((void *)0x02000000, 0x10000000, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        fprintf(stderr, "[tail] test: 0x02000000-0x12000000 %s\n", p == (void *)0x02000000 ? "guarded" : "not free");
        if (p != (void *)0x02000000) print_taken(0x02000000u, 0x12000000u);
    }
    return 0;
}

/* pe_loader.c, once the sections are in memory. */
int recomp_pe_fixup(void) {
    uint32_t at = 0x00400000u, d = 0, sh = 0, n = 0;
    for (size_t i = 0; i < sizeof k_table; i++) {
        d |= (uint32_t)(k_table[i] & 127) << sh;
        sh += 7;
        if (k_table[i] & 128) continue;
        at += d;
        d = sh = 0;
        uint32_t v;
        memcpy(&v, (const void *)(uintptr_t)at, 4);
        if (v >= 0x00B69000u && v < 0x00B6D000u) continue;   /* an import slot of the Remastered's: in .rdata, not moved */
        if ((v < GAME_TAIL_CUT || v >= GAME_DATA_OLD_HI) && (v < 0x027C10B4u || v >= 0x027C10D4u)) {   /* ...the 2013 release's are moved */
            fprintf(stderr, "[tail] FATAL: 0x%08X holds 0x%08X: not the game program this build is for\n", at, v);
            return -1;
        }
        v += GAME_TAIL_DELTA;
        memcpy((void *)(uintptr_t)at, &v, 4);
        n++;
    }
    fprintf(stderr, "[tail] game memory from 0x%08X moved to 0x%08X (%u addresses)\n", GAME_TAIL_CUT, GAME_TAIL_CUT + GAME_TAIL_DELTA, n);
    return 0;
}

/* Is the 32-bit value at `va` one of the table's addresses? (ff8_hext.cpp) */
int ff8_tail_is_site(uint32_t va) {
    uint32_t at = 0x00400000u, d = 0, sh = 0;
    for (size_t i = 0; i < sizeof k_table && at < va; i++) {
        d |= (uint32_t)(k_table[i] & 127) << sh;
        sh += 7;
        if (k_table[i] & 128) continue;
        at += d;
        d = sh = 0;
    }
    return at == va;
}
