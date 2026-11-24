/*
 * ff8_runtime_hooks.c - the host side of recomp_types.h's contract for FF7.
 *
 * Every way the lifted code can leave the lifted model unexpectedly stops the run and
 * says where: an indirect call/jump to an address nothing lifted (RECOMP_STRICT_ICALL),
 * an instruction the lifter could not express, or a hook site with no handler.
 * No silent fallbacks.
 */
#include <stdio.h>
#include <stdlib.h>
#include "recomp_types.h"
#include "win32hle.h"
#include "game_image.h"

extern const char *g_cur_import;   /* host_lite.c: the shim being run, for diagnostics */
void ff7_trace_dump(int last);     /* ff8_trace.c */

static void dump_regs(void) {
    fprintf(stderr,
            "  eax=%08X ebx=%08X ecx=%08X edx=%08X\n"
            "  esi=%08X edi=%08X ebp=%08X esp=%08X  fs=%08X\n"
            "  in func 0x%08X, last import %s\n",
            g_eax, g_ebx, g_ecx, g_edx, g_esi, g_edi, g_ebp, g_esp, g_fs_base,
            g_cur_func, g_cur_import ? g_cur_import : "(none)");
    fprintf(stderr, "  recent indirect calls (target <- from):\n");
    for (uint32_t i = 0; i < 8 && i < g_icall_count; i++) {
        uint32_t k = (g_icall_trace_idx - 1 - i) & (ICALL_TRACE_SIZE - 1);
        const char *nm = hle_name(g_icall_trace[k]);
        fprintf(stderr, "    0x%08X <- 0x%08X%s%s\n", g_icall_trace[k], g_icall_from[k], nm ? "  " : "", nm ? nm : "");
    }
    /* guest memory at the registers that point into the game's image (0x400000 up to 0x27A0000):
     * a corrupted object shows its neighbours (the bad pointer an ICALL took came from one) */
    {
        const uint32_t regs[] = { g_esi, g_edi, g_ebx, g_ebp, g_ecx, g_edx };
        const char *names[] = { "esi", "edi", "ebx", "ebp", "ecx", "edx" };
        for (int r = 0; r < 6; r++) {
            const uint32_t a = regs[r] & ~3u;
            if (!((a >= 0x00401000u && a < GAME_IMAGE_HI - 0xD0u) || (a >= GAME_TAIL_LO + 0x10u && a < GAME_TAIL_HI - 0xD0u))) continue;
            fprintf(stderr, "  [%s-0x10] %08X:", names[r], a - 0x10);   /* 48 dwords: two 0x5C task nodes */
            for (int k = 0; k < 48; k++) fprintf(stderr, " %08X", *(const uint32_t *)(uintptr_t)(a - 0x10 + 4 * k));
            fprintf(stderr, "\n");
        }
    }
    ff7_trace_dump(64);
}

void recomp_unresolved(const char *kind, uint32_t va, uint32_t from) {
    const char *shim = hle_name(va);
    fprintf(stderr, "[ff7] STOP: %s to unlifted VA 0x%08X from func 0x%08X%s%s\n",
            kind, va, from, shim ? " (shim " : "", shim ? shim : "");
    dump_regs();
    exit(90);
}

void recomp_unimpl(uint32_t va, const char *what) {
    fprintf(stderr, "[ff7] STOP: unimplemented '%s' at 0x%08X\n", what, va);
    dump_regs();
    exit(91);
}

void recomp_hook(uint32_t va) {
    fprintf(stderr, "[ff7] STOP: hook site 0x%08X has no handler\n", va);
    dump_regs();
    exit(92);
}

void recomp_dump_trace(const char *why) {
    fprintf(stderr, "[ff7] trace dump: %s\n", why ? why : "");
    dump_regs();
}
