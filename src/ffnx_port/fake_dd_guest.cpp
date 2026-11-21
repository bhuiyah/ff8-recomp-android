// fake_dd_guest.cpp - FFNx's fake DirectDraw / Direct3D objects (fake_dd.cpp) as the game sees
// them: COM objects in guest memory.
//
// FF8 talks to DirectDraw through COM: it holds an interface pointer (the address of a cell
// that holds the vtable's address) and calls (*obj)->method(obj, args...) as stdcall. FFNx
// hands it its own fake objects (ff8_init_hooks: game_object->dddevice = &_fake_dddevice).
// On Windows those are FFNx's statics; here the game needs 32-bit addresses, so each fake
// object gets a guest cell and a guest vtable whose slots are stdcall thunks of FFNx's
// functions. The slot numbers follow fake_dd.h's 32-bit layout (and fake_dd.cpp's tables).
//
// ffnx_fake_dd_va(&_fake_dddevice) and friends: the guest interface pointer for one of
// fake_dd.cpp's globals; tools/ffnx_edit_ff8.py writes these where FFNx stores the objects.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <utility>

#include "ddraw.h"
#include "guest_abi.h"
#include "fake_dd.h"

// fake_dd.cpp's functions (declared there, not in its header)
uint32_t __stdcall fake_dd_blit_fast(struct ddsurface **me, uint32_t unknown1, uint32_t unknown2, struct ddsurface **target, LPRECT source, uint32_t unknown3);
uint32_t __stdcall fake_ddsurface_get_pixelformat(struct ddsurface **me, LPDDPIXELFORMAT pf);
uint32_t __stdcall fake_ddsurface_get_surface_desc(struct ddsurface **me, LPDDSURFACEDESC2 sd);
uint32_t __stdcall fake_ddsurface_get_dd_interface(struct ddsurface **me, struct dddevice ***dd);
uint32_t __stdcall fake_ddsurface_get_palette(struct ddsurface **me, void **palette);
uint32_t __stdcall fake_ddsurface_lock(struct ddsurface **me, LPRECT dest, LPDDSURFACEDESC sd, uint32_t flags, uint32_t unused);
uint32_t __stdcall fake_ddsurface_unlock(struct ddsurface **me, LPRECT dest);
uint32_t __stdcall fake_ddsurface_islost(struct ddsurface **me);
uint32_t __stdcall fake_ddsurface_restore(struct ddsurface **me);
uint32_t __stdcall fake_dd_query_interface(struct dddevice **me, uint32_t *iid, void **ppvobj);
uint32_t __stdcall fake_dd_addref(struct dddevice **me);
uint32_t __stdcall fake_dd_release(struct dddevice **me);
uint32_t __stdcall fake_dd_create_clipper(struct dddevice **me, DWORD flags, LPDIRECTDRAWCLIPPER *clipper);
uint32_t __stdcall fake_dd_create_palette(struct dddevice **me, LPPALETTEENTRY palette_entry, LPDIRECTDRAWPALETTE *palette, IUnknown *unused);
uint32_t __stdcall fake_dd_create_surface(struct dddevice **me, LPDDSURFACEDESC sd, LPDIRECTDRAWSURFACE *surface, IUnknown *unused);
uint32_t __stdcall fake_dd_get_caps(struct dddevice **me, LPDDCAPS halcaps, LPDDCAPS helcaps);
uint32_t __stdcall fake_dd_get_display_mode(struct dddevice **me, LPDDSURFACEDESC sd);
uint32_t __stdcall fake_dd_set_coop_level(struct dddevice **me, uint32_t coop_level);
uint32_t __stdcall fake_d3d_get_caps(struct d3d2device **me, void *a, void *b);

namespace {

// A stdcall thunk per native function: COM methods pop their own arguments (this included).
template <typename Sig> struct stdcall_pool;
template <typename R, typename... A> struct stdcall_pool<R(A...)> {
    using fp = R (*)(A...);
    static constexpr int N = 8;
    static inline fp fns[N];
    static inline uint32_t vas[N];
    static inline int used;
    template <int I> static void thunk() { guest::run_export<R, A...>(fns[I], true); }
    template <int... I>
    static constexpr std::array<recomp_func_t, N> make(std::integer_sequence<int, I...>) { return { &thunk<I>... }; }
    static uint32_t va_for(fp f) {
        static const std::array<recomp_func_t, N> thunks = make(std::make_integer_sequence<int, N>{});
        for (int i = 0; i < used; i++) if (fns[i] == f) return vas[i];
        if (used == N) guest::fatal("fake_dd: stdcall pool exhausted", (uintptr_t)f);
        int i = used++;
        fns[i] = f;
        char *name = static_cast<char *>(std::malloc(48));
        std::snprintf(name, 48, "fake_dd:%p", reinterpret_cast<void *>(f));
        win32hle_shim one[2] = { { name, thunks[i] }, { nullptr, nullptr } };
        win32hle_register(one);
        vas[i] = hle_resolve(name);
        return vas[i];
    }
};
template <typename R, typename... A> uint32_t st(R (*f)(A...)) { return stdcall_pool<R(A...)>::va_for(f); }

// A guest COM object: [cell] -> vtable of n slots.
struct guest_obj { uint32_t cell_va; uint32_t *vtbl; };
guest_obj make_obj(int slots) {
    uint32_t *vt = static_cast<uint32_t *>(guest_calloc(slots, 4));
    uint32_t *cell = static_cast<uint32_t *>(guest_calloc(1, 4));
    if (!vt || !cell) guest::fatal("fake_dd: guest heap exhausted", slots);
    *cell = guest::va_of(vt);
    return { guest::va_of(cell), vt };
}

void fill_surface(uint32_t *v) {   // fake_dd.h struct ddsurface, 37 slots
    v[0] = st(fake_dd_query_interface);
    v[1] = st(fake_dd_addref);
    v[2] = st(fake_dd_release);
    v[7] = st(fake_dd_blit_fast);
    v[20] = st(fake_ddsurface_get_palette);
    v[21] = st(fake_ddsurface_get_pixelformat);
    v[22] = st(fake_ddsurface_get_surface_desc);
    v[24] = st(fake_ddsurface_islost);
    v[25] = st(fake_ddsurface_lock);
    v[27] = st(fake_ddsurface_restore);
    v[32] = st(fake_ddsurface_unlock);
    v[36] = st(fake_ddsurface_get_dd_interface);
}

struct objects {
    guest_obj back, front, temp, device, d3d;
    objects() {
        back = make_obj(37); fill_surface(back.vtbl);
        front = make_obj(37); fill_surface(front.vtbl);
        temp = make_obj(37); fill_surface(temp.vtbl);
        device = make_obj(24);                       // struct dddevice
        device.vtbl[0] = st(fake_dd_query_interface);
        device.vtbl[1] = st(fake_dd_addref);
        device.vtbl[2] = st(fake_dd_release);
        device.vtbl[4] = st(fake_dd_create_clipper);
        device.vtbl[5] = st(fake_dd_create_palette);
        device.vtbl[6] = st(fake_dd_create_surface);
        device.vtbl[11] = st(fake_dd_get_caps);
        device.vtbl[12] = st(fake_dd_get_display_mode);
        device.vtbl[20] = st(fake_dd_set_coop_level);
        d3d = make_obj(31);                          // struct d3d2device (junk[10] at slot 16)
        d3d.vtbl[0] = st(fake_dd_query_interface);
        d3d.vtbl[1] = st(fake_dd_addref);
        d3d.vtbl[2] = st(fake_dd_release);
        d3d.vtbl[3] = st(fake_d3d_get_caps);
    }
};
objects &all() { static objects o; return o; }   // built on first use: the guest heap is up by then

}  // namespace

// The guest interface pointer standing for one of fake_dd.cpp's interface globals.
uint32_t ffnx_fake_dd_va(const void *host_global) {
    objects &o = all();
    if (host_global == &_fake_dd_back_surface) return o.back.cell_va;
    if (host_global == &_fake_dd_front_surface) return o.front.cell_va;
    if (host_global == &_fake_dd_temp_surface) return o.temp.cell_va;
    if (host_global == &_fake_dddevice) return o.device.cell_va;
    if (host_global == &_fake_d3d2device) return o.d3d.cell_va;
    guest::fatal("ffnx_fake_dd_va: not a fake_dd interface", (uintptr_t)host_global);
}
