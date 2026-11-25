#pragma once
// src/ffnx_port/demaster.cpp: textures of Demaster mods
#ifdef __cplusplus
extern "C" {
#endif
/* A Demaster mod's file for a texture page FFNx calls `name`, at VRAM page `page`, or NULL. *paletted:
 * it is for this palette alone. */
const char *ff8_demaster_page(const char *name, int page, unsigned palette, int *paletted, char *buf, unsigned n);
/* A mod's .dds in place of a .png in the Remastered's archive ("textures\\..."), or NULL. */
const char *ff8_demaster_dds(const char *archived, char *buf, unsigned n);
#ifdef __cplusplus
}
#endif
