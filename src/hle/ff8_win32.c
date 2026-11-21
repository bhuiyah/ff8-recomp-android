/*
 * ff8_win32.c - the Win32 imports FF8_EN.exe has beyond FF7's (src/hle/ff7_*.c cover the rest).
 *
 * FF8 is single-threaded (no CreateThread): its mutex and wait calls only guard against a
 * second running copy, and its process calls launch the Steam launcher, which Android has
 * not. The GDI drawing calls belong to the 2000 renderer's debug text and the software
 * path FFNx replaces; they answer "done" and draw nothing. The registry calls answer the
 * way the AF3DN.P ones do (ff7_af3dn.c): one configuration, FFNx's.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "win32hle.h"
#include "ff7_hle.h"

#define W32(p) (*(uint32_t *)(uintptr_t)(p))

/* ---- KERNEL32 ------------------------------------------------------------------------ */

/* QueryPerformance*: a 10 MHz counter (Windows' usual QPC frequency) over CLOCK_MONOTONIC. */
static void k_QueryPerformanceFrequency(void) {
    uint32_t p = A32(0);
    if (p) { W32(p) = 10000000u; W32(p + 4) = 0; }
    RET(1, 1);
}
static void k_QueryPerformanceCounter(void) {
    uint32_t p = A32(0);
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t ticks = (uint64_t)t.tv_sec * 10000000ull + (uint64_t)t.tv_nsec / 100u;
    if (p) { W32(p) = (uint32_t)ticks; W32(p + 4) = (uint32_t)(ticks >> 32); }
    RET(1, 1);
}

/* The single-instance mutex: always the first copy. */
static void k_CreateMutexA(void)       { RET(0x0000AB01u, 3); }   /* (sa, initialOwner, name) */
static void k_ReleaseMutex(void)       { RET(1, 1); }
static void k_WaitForSingleObject(void){ RET(0, 2); }             /* WAIT_OBJECT_0 */
static void k_CreateProcessA(void) {                               /* 10 args */
    const char *app = ASTR(0), *cmd = ASTR(1);
    fprintf(stderr, "[ff8] CreateProcessA(%s, %s) refused: no other programs here\n",
            app ? app : "(null)", cmd ? cmd : "(null)");
    RET(0, 10);
}
static void k_TerminateProcess(void) {
    fprintf(stderr, "[ff8] TerminateProcess(exit %u)\n", A32(1));
    exit((int)A32(1));
}
static void k_GetCurrentThread(void)   { RET(0xFFFFFFFEu, 0); }   /* the pseudo-handle */
static void k_GetPriorityClass(void)   { RET(0x00000020u, 1); }   /* NORMAL_PRIORITY_CLASS */
static void k_SetPriorityClass(void)   { RET(1, 2); }
static void k_GetThreadPriority(void)  { RET(0, 1); }             /* THREAD_PRIORITY_NORMAL */
static void k_SetThreadPriority(void)  { RET(1, 2); }

/* ---- ADVAPI32: the same answers as AF3DN.P's dotemuReg* (ff7_af3dn.c) -------------------- */
extern const win32hle_shim ff7_af3dn[];
static recomp_func_t af3dn(const char *name) {
    for (const win32hle_shim *s = ff7_af3dn; s->name; s++)
        if (!strcmp(s->name, name)) return s->fn;
    hle_fatal("ff8_win32: no %s in ff7_af3dn", name);
    return NULL;
}
static void a_RegOpenKeyExA(void)    { af3dn("dotemuRegOpenKeyExA")(); }
static void a_RegCloseKey(void)      { af3dn("dotemuRegCloseKey")(); }
static void a_RegFlushKey(void)      { af3dn("dotemuRegFlushKey")(); }
static void a_RegSetValueExA(void)   { af3dn("dotemuRegSetValueExA")(); }
static void a_RegQueryValueExA(void) { af3dn("dotemuRegQueryValueExA")(); }

/* ---- WINMM: MCI (the Steam 2013 exe plays "Eyes on Me" through it; FFNx replaces the call
 * with its own player, ff8 dotemuMciSendCommandA) ------------------------------------------ */
static void m_mciSendCommandA(void)    { RET(0x0000000Bu, 4); }   /* MCIERR_DEVICE_NOT_READY... */
static void m_mciGetErrorStringA(void) {
    char *buf = ASTR(1);
    uint32_t n = A32(2);
    if (buf && n) buf[0] = 0;
    RET(1, 3);
}

/* ---- USER32 --------------------------------------------------------------------------- */
#define SCREEN_W 640
#define SCREEN_H 480
static void u_LoadIconA(void)      { RET(0x00C1C000u + (A32(1) & 0xFFFFu), 2); }
static void u_SetWindowPos(void)   { RET(1, 7); }
static void u_GetRect(void) {       /* GetWindowRect / GetClientRect (hwnd, rect) */
    uint32_t r = A32(1);
    if (r) { W32(r) = 0; W32(r + 4) = 0; W32(r + 8) = SCREEN_W; W32(r + 12) = SCREEN_H; }
    RET(1, 2);
}

/* ---- GDI32 ---------------------------------------------------------------------------- */
static void g_CreatePen(void)         { RET(0x00C2C001u, 3); }
static void g_TextOutA(void)          { RET(1, 5); }
static void g_SetBkColor(void)        { RET(0, 2); }
static void g_SetTextColor(void)      { RET(0, 2); }
static void g_MoveToEx(void)          { RET(1, 4); }
static void g_LineTo(void)            { RET(1, 3); }
static void g_SetDIBitsToDevice(void) { RET(A32(8), 12); }   /* lines drawn = cLines */
static void g_ChoosePixelFormat(void) { RET(1, 2); }
static void g_SetPixelFormat(void)    { RET(1, 3); }
static void g_GetPixelFormat(void)    { RET(1, 1); }
static void g_DescribePixelFormat(void){ RET(1, 4); }        /* one format */
static void g_SwapBuffers(void)       { RET(1, 1); }

/* ---- EAX.DLL #5 (EAXDirectSoundCreate): no EAX, so plain DirectSound, as FFNx's export
 * falls back to when eax.dll has none (common.cpp EAXDirectSoundCreate) -------------------- */
extern const win32hle_shim ff7_dsound[];
static void e_EAXDirectSoundCreate(void) {   /* (guid, lplpds, unk): DirectSoundCreate's args */
    for (const win32hle_shim *s = ff7_dsound; s->name; s++)
        if (!strcmp(s->name, "DirectSoundCreate")) { s->fn(); return; }
    hle_fatal("ff8_win32: no DirectSoundCreate");
}

const win32hle_shim ff8_win32[] = {
    { "QueryPerformanceFrequency", k_QueryPerformanceFrequency },
    { "QueryPerformanceCounter", k_QueryPerformanceCounter },
    { "CreateMutexA", k_CreateMutexA },
    { "ReleaseMutex", k_ReleaseMutex },
    { "WaitForSingleObject", k_WaitForSingleObject },
    { "CreateProcessA", k_CreateProcessA },
    { "TerminateProcess", k_TerminateProcess },
    { "GetCurrentThread", k_GetCurrentThread },
    { "GetPriorityClass", k_GetPriorityClass },
    { "SetPriorityClass", k_SetPriorityClass },
    { "GetThreadPriority", k_GetThreadPriority },
    { "SetThreadPriority", k_SetThreadPriority },
    { "RegOpenKeyExA", a_RegOpenKeyExA },
    { "RegCloseKey", a_RegCloseKey },
    { "RegFlushKey", a_RegFlushKey },
    { "RegSetValueExA", a_RegSetValueExA },
    { "RegQueryValueExA", a_RegQueryValueExA },
    { "mciSendCommandA", m_mciSendCommandA },
    { "mciGetErrorStringA", m_mciGetErrorStringA },
    { "LoadIconA", u_LoadIconA },
    { "SetWindowPos", u_SetWindowPos },
    { "GetWindowRect", u_GetRect },
    { "GetClientRect", u_GetRect },
    { "CreatePen", g_CreatePen },
    { "TextOutA", g_TextOutA },
    { "SetBkColor", g_SetBkColor },
    { "SetTextColor", g_SetTextColor },
    { "MoveToEx", g_MoveToEx },
    { "LineTo", g_LineTo },
    { "SetDIBitsToDevice", g_SetDIBitsToDevice },
    { "ChoosePixelFormat", g_ChoosePixelFormat },
    { "SetPixelFormat", g_SetPixelFormat },
    { "GetPixelFormat", g_GetPixelFormat },
    { "DescribePixelFormat", g_DescribePixelFormat },
    { "SwapBuffers", g_SwapBuffers },
    { "EAX.DLL#5", e_EAXDirectSoundCreate },
    { NULL, NULL }
};
