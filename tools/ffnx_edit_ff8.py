"""FF8Recomp: source edits to FFNx's FF8 files for the 64-bit guest boundary, applied by
tools/ffnx_edit_android.py with its FF7 ones (same format: (file, old, new[, 'all'])).

What they fix, by kind:
- pointer truncation: FFNx's FF8 code turns pointers into 32-bit values with uint32_t(p),
  (DWORD)p, reinterpret_cast<uint32_t>(p). On arm64 that cuts a host address. Each becomes
  ffnx_va(p) (src/ffnx_port/include/ffnx_guest.h): a guest pointer passes, a native
  function becomes a guest-callable thunk, anything else stops the run naming the address.
  cmake/ffnx.cmake makes any cast left over a compile error (-Werror=pointer-to-int-cast).
- FFNx data the game reads (code patches point it there): moved into the fixed guest block.
- double pointers a game function fills: guest cells (gptr<T>) instead of host locals.
- game-function pointers kept by FFNx: guest::gfn.
"""

FF8_EDITS = [
    # The FF8 driver table: these slots hold FFNx's functions (ff8_load_driver).
    ('ff8.h',
     '\tgptr<void> field_54;\n\tgptr<void> field_58;\n\tgptr<void> field_5C;\n\tgptr<void> field_60;\n',
     '\tgfn<void(struct texture_set *, struct game_obj *)> field_54;   // FF8Recomp: FFNx functions\n'
     '\tgfn<void(struct texture_set *, struct game_obj *)> field_58;\n'
     '\tgfn<void(struct texture_set *, struct game_obj *)> field_5C;\n'
     '\tgfn<void(struct palette *, struct texture_set *)> field_60;\n'),
    ('ff8.h', '\tgptr<void> field__84;\n\tgptr<void> field_88;\n',
     '\tgfn<void(uint32_t, struct game_obj *)> field__84;   // FF8Recomp: FFNx functions\n'
     '\tgfn<void()> field_88;\n'),

    # ---- ff8_opengl.cpp
    ('ff8_opengl.cpp', 'reinterpret_cast<uint8_t *>(*ff8_externals.effect_struct_27973EC)',
     '(uint8_t *)(*ff8_externals.effect_struct_27973EC).get()'),
    ('ff8_opengl.cpp', 'reinterpret_cast<nvertex *>(graphics_object->field_74)',
     '(nvertex *)graphics_object->field_74.get()'),
    # extended_memory comes from driver_malloc: the guest heap
    ('ff8_opengl.cpp', 'uint32_t(extended_memory)', 'ffnx_va(extended_memory)', 'all'),
    ('ff8_opengl.cpp', 'static int (*ff8_field_key_tests[4])(ff8_field_state_common *);',
     'static guest::gfn<int(ff8_field_state_common *)> ff8_field_key_tests[4];   // FF8Recomp: game functions'),
    ('ff8_opengl.cpp',
     'patch_code_dword(uint32_t(&common_externals.execute_opcode_table[opcodes[i]]), uint32_t(handlers[i]));',
     'patch_code_dword(ffnx_va(&common_externals.execute_opcode_table[opcodes[i]]), ffnx_va(handlers[i]));'),
    ('ff8_opengl.cpp', 'static const double ff8_swirl_half_pixel_double = 0.5;',
     'static const double &ff8_swirl_half_pixel_double = ffnx_guest_static<double>(0.5);   // FF8Recomp: guest block'),
    ('ff8_opengl.cpp', 'static const float ff8_swirl_half_pixel_float = 0.5f;',
     'static const float &ff8_swirl_half_pixel_float = ffnx_guest_static<float>(0.5f);   // FF8Recomp: guest block'),
    ('ff8_opengl.cpp', 'uint32_t(&ff8_swirl_half_pixel_', 'ffnx_va(&ff8_swirl_half_pixel_', 'all'),
    ('ff8_opengl.cpp', 'uint32_t(ff8_main_menu_controller)', 'ffnx_va(ff8_main_menu_controller)'),
    ('ff8_opengl.cpp', 'int(ff8_externals.vibrate_data_summon_quezacotl)',
     'int(ffnx_va(ff8_externals.vibrate_data_summon_quezacotl))', 'all'),
    ('ff8_opengl.cpp', '(uint32_t)&common_externals.execute_opcode_table[0x69],',
     'ffnx_va(&common_externals.execute_opcode_table[0x69]),'),
    ('ff8_opengl.cpp', '(uint32_t)&common_externals.execute_opcode_table[0x13A],',
     'ffnx_va(&common_externals.execute_opcode_table[0x13A]),'),
    ('ff8_opengl.cpp', '(uint32_t)&common_externals.execute_opcode_table[0x0D],',
     'ffnx_va(&common_externals.execute_opcode_table[0x0D]),'),
    ('ff8_opengl.cpp', '(uint32_t)&common_externals.execute_opcode_table[0x153],',
     'ffnx_va(&common_externals.execute_opcode_table[0x153]),'),
    ('ff8_opengl.cpp', '(uint32_t)&common_externals.execute_opcode_table[0x151],',
     'ffnx_va(&common_externals.execute_opcode_table[0x151]),'),
    ('ff8_opengl.cpp', '(uint32_t)&common_externals.execute_opcode_table[0x0B],',
     'ffnx_va(&common_externals.execute_opcode_table[0x0B]),'),
    ('ff8_opengl.cpp', '(uint32_t)&ff8_externals.cardgame_funcs[4],',
     'ffnx_va(&ff8_externals.cardgame_funcs[4]),'),
    ('ff8_opengl.cpp', 'reinterpret_cast<uint32_t>(+process_battle_effect_scripts)', 'ffnx_va(+process_battle_effect_scripts)'),
    ('ff8_opengl.cpp', 'caller, uint32_t(descriptor), uint32_t(records), stride',
     'caller, ffnx_trace_ptr(descriptor), ffnx_trace_ptr(records), stride'),

    # ---- FFNx's fake DirectDraw objects: the game gets guest COM objects
    # (src/ffnx_port/fake_dd_guest.cpp), and their methods write 4-byte guest cells.
    ('ff8_opengl.cpp', '\tgame_object->dddevice = &_fake_dddevice;\n\tgame_object->front_surface[0] = &_fake_dd_front_surface;\n'
                       '\tgame_object->front_surface[1] = &_fake_dd_back_surface;\n\tgame_object->dd2interface = &_fake_dddevice;\n'
                       '\tgame_object->d3d2device = &_fake_d3d2device;\n',
     '\t// FF8Recomp: guest COM objects (src/ffnx_port/fake_dd_guest.cpp)\n'
     '\tgame_object->dddevice = ffnx_fake_dd_va(&_fake_dddevice);\n\tgame_object->front_surface[0] = ffnx_fake_dd_va(&_fake_dd_front_surface);\n'
     '\tgame_object->front_surface[1] = ffnx_fake_dd_va(&_fake_dd_back_surface);\n\tgame_object->dd2interface = ffnx_fake_dd_va(&_fake_dddevice);\n'
     '\tgame_object->d3d2device = ffnx_fake_dd_va(&_fake_d3d2device);\n'),
    # fake_dd.cpp's own tables are host-only here (the game gets fake_dd_guest.cpp's): their
    # function slots take any function, as MSVC's void * did.
    ('fake_dd.h', '#include <stdint.h>\n',
     '#include <stdint.h>\n\n'
     '// FF8Recomp: a vtable slot of FFNx\'s host-side tables (the game sees src/ffnx_port/fake_dd_guest.cpp\'s)\n'
     'struct fake_fn { void *p = nullptr; fake_fn() = default; fake_fn(int) {} template <class F> fake_fn(F *f) : p((void *)f) {} };\n'),
    ('fake_dd.h', '\tvoid *', '\tfake_fn ', 'all'),
    ('fake_dd.cpp', '\t*dd = &_fake_dddevice;', '\t*(uint32_t *)dd = ffnx_fake_dd_va(&_fake_dddevice);   // FF8Recomp: a 4-byte cell'),
    ('fake_dd.cpp', '\t\t*ppvobj = me;', '\t\t*(uint32_t *)ppvobj = ffnx_va(me);   // FF8Recomp: a 4-byte cell'),
    ('fake_dd.cpp', '\t\t*ppvobj = &_fake_dd_temp_surface;', '\t\t*(uint32_t *)ppvobj = ffnx_fake_dd_va(&_fake_dd_temp_surface);'),
    ('fake_dd.cpp', '\t*clipper = 0;', '\t*(uint32_t *)clipper = 0;   // FF8Recomp: a 4-byte cell'),
    ('fake_dd.cpp', '\t*palette = 0;', '\t*(uint32_t *)palette = 0;   // FF8Recomp: a 4-byte cell'),
    ('fake_dd.cpp', '\t*surface = (LPDIRECTDRAWSURFACE)&_fake_dd_temp_surface;',
     '\t*(uint32_t *)surface = ffnx_fake_dd_va(&_fake_dd_temp_surface);   // FF8Recomp: a 4-byte cell'),

    # ---- ff8_data.cpp: the zoom function's own type (guest cells for its double pointers)
    ('ff8_data.cpp',
     'ff8_externals.world_calc_zoom_sub_50F900 = guest::gfn<int16_t(int16_t *, int16_t **, uint32_t **)>::from_va(',
     'ff8_externals.world_calc_zoom_sub_50F900 = decltype(ff8_externals.world_calc_zoom_sub_50F900)::from_va('),

    # ---- battle/effect_camera.cpp
    ('ff8/battle/effect_camera.cpp', 'uint32_t(_ReturnAddress())', 'guest::caller_va()'),
    ('ff8/battle/effect_camera.cpp', 'static ff8_magic_effect_init effect_camera_original_init = nullptr;',
     'static guest::gfn<int(ff8_battle_magic_cast_context *)> effect_camera_original_init;   // FF8Recomp: a game function'),
    ('ff8/battle/effect_camera.cpp', '\tff8_magic_effect_init init = effect_camera_original_init;',
     '\tauto init = effect_camera_original_init;'),
    ('ff8/battle/effect_camera.cpp',
     'static void *ff8_effect_camera_read_effect(int effect_id, ff8_magic_effect_init *init)',
     'static void *ff8_effect_camera_read_effect(int effect_id, uint32_t *init)   // FF8Recomp: a guest cell holding a game function'),
    ('ff8/battle/effect_camera.cpp', 'guest::gfn<void *(int, ff8_magic_effect_init *)>',
     'guest::gfn<void *(int, uint32_t *)>'),
    ('ff8/battle/effect_camera.cpp',
     '\tif (*init != nullptr)\n\t{\n\t\teffect_camera_original_init = *init;',
     '\tif (*init != 0)\n\t{\n\t\teffect_camera_original_init = guest::gfn<int(ff8_battle_magic_cast_context *)>::from_va(*init);'),
    ('ff8/battle/effect_camera.cpp', '\t\t*init = ff8_effect_camera_effect_start;',
     '\t\t*init = ffnx_va(ff8_effect_camera_effect_start);'),

    # ---- battle/stage.cpp: compares two pointers into the same buffer
    ('ff8/battle/stage.cpp', 'uint32_t(pos_next_model) > uint32_t(after_vertices)', 'pos_next_model > after_vertices'),

    # ---- esc_menu.cpp: the ordering table and sprites the game's draw calls link together
    ('ff8/esc_menu.cpp', 'static uint32_t ot = 0;',
     'static uint32_t &ot = ffnx_guest_static<uint32_t>(0u);   // FF8Recomp: guest block'),
    ('ff8/esc_menu.cpp', 'static ff8_draw_menu_sprite_texture_infos sprites[256];',
     'static ff8_draw_menu_sprite_texture_infos (&sprites)[256] = ffnx_guest_static_zero<ff8_draw_menu_sprite_texture_infos, 256>();   // FF8Recomp: guest block'),
    ('ff8/esc_menu.cpp', 'draw_menu(int(&ot),', 'draw_menu(int(ffnx_va(&ot)),'),

    # ---- fonts.cpp
    ('ff8/fonts.cpp', 'memcpy_code(uint32_t(font_alignment_data),', 'memcpy_code(ffnx_va(font_alignment_data),'),
    ('ff8/fonts.cpp', 'memcpy_code(uint32_t(font_character_width_local_field),',
     'memcpy_code(ffnx_va(font_character_width_local_field),'),
    ('ff8/fonts.cpp', 'texture_infos = ff8_draw_icon_or_key1(\n                int(a1),',
     'texture_infos = ff8_draw_icon_or_key1(\n                int(ffnx_va(a1)),'),
    ('ff8/fonts.cpp', 'ff8_draw_icon_or_key1(int(a1), a2', 'ff8_draw_icon_or_key1(int(ffnx_va(a1)), a2'),
    ('ff8/fonts.cpp', 'void *buffer2 = nullptr;', 'gptr<int> buffer2 = nullptr;   // FF8Recomp: a guest cell the game fills', 'all'),
    ('ff8/fonts.cpp', '(int **)&buffer2', '&buffer2 /* FF8Recomp: guest cell */', 'all'),
    ('ff8/fonts.cpp', 'replace_function(reinterpret_cast<uint32_t>(ff8_externals.get_character_width),',
     'replace_function(ffnx_va(ff8_externals.get_character_width),'),
    # the game function writes a 32-bit count (size_t on x86)
    ('ff8/fonts.cpp', '    size_t element_count;', '    uint32_t element_count;   // FF8Recomp: 4 bytes, size_t on x86'),

    # ---- texture_packer.cpp: tells a small id from a name pointer
    ('ff8/texture_packer.cpp', 'if ((uint32_t)textureName > 32)', 'if ((uintptr_t)textureName > 32)'),

    # ---- uv_patch.cpp: the world map's polygon condition, a game function pointer in game memory
    ('ff8/uv_patch.cpp',
     '\t*(void **)ff8_externals.worldmap_polygon_condition_2045C8C = current_polygon_condition_old;',
     '\t*(uint32_t *)ff8_externals.worldmap_polygon_condition_2045C8C = (uint32_t)(uintptr_t)current_polygon_condition_old;   // FF8Recomp: a 4-byte cell'),
    ('ff8/uv_patch.cpp', '::from_va((uint32_t)(current_polygon_condition_old))',
     '::from_va((uint32_t)(uintptr_t)(current_polygon_condition_old))'),
    ('ff8/uv_patch.cpp',
     '\tvoid **current_polygon_condition_dword_2045964 = (void **)ff8_externals.worldmap_polygon_condition_2045C8C;',
     '\tuint32_t *current_polygon_condition_dword_2045964 = (uint32_t *)ff8_externals.worldmap_polygon_condition_2045C8C;   // FF8Recomp: a 4-byte cell'),
    ('ff8/uv_patch.cpp', '\t\tcurrent_polygon_condition_old = *current_polygon_condition_dword_2045964;',
     '\t\tcurrent_polygon_condition_old = (void *)(uintptr_t)*current_polygon_condition_dword_2045964;'),
    ('ff8/uv_patch.cpp', '\t\t*current_polygon_condition_dword_2045964 = current_polygon_condition;',
     '\t\t*current_polygon_condition_dword_2045964 = ffnx_va(current_polygon_condition);'),
    ('ff8/uv_patch.cpp', 'ff8_externals.ssigpu_callbacks_1[52] = uint32_t(ssigpu_callback_sub_461E00);',
     'ff8_externals.ssigpu_callbacks_1[52] = ffnx_va(ssigpu_callback_sub_461E00);'),

    # ---- vram.cpp
    ('ff8/vram.cpp', 'effect_id, uint32_t(data), ret);', 'effect_id, ffnx_trace_ptr(data), ret);'),
    ('ff8/vram.cpp', '\t\t\t\tuint32_t(data));', '\t\t\t\tffnx_trace_ptr(data));'),
    ('ff8/vram.cpp', 'int(target)', 'int(ffnx_trace_ptr(target))', 'all'),
    ('ff8/vram.cpp', 'int(vram_palette)', 'int(ffnx_trace_ptr(vram_palette))', 'all'),
    ('ff8/vram.cpp', 'DWORD(texture_data)', 'ffnx_va(texture_data)', 'all'),
    ('ff8/vram.cpp', 'current_tex_page->sub_tri_gfxobj == nullptr ? current_tex_page->sub_tex_header : ',
     'current_tex_page->sub_tri_gfxobj == nullptr ? (ff8_tex_header *)current_tex_page->sub_tex_header : '),

    # ---- widescreen.cpp
    ('ff8/widescreen.cpp',
     'int16_t ff8_battle_world_calc_zoom(int16_t *descriptor, int16_t **vertices, uint32_t **projected_vertices)',
     'int16_t ff8_battle_world_calc_zoom(int16_t *descriptor, gptr<int16_t> *vertices, gptr<uint32_t> *projected_vertices)   // FF8Recomp: guest cells'),
    ('ff8/widescreen.cpp', 'static int (*ff8_worldmap_model_store_depths)(uint32_t *, uint32_t *, uint32_t *);',
     'static guest::gfn<int(uint32_t *, uint32_t *, uint32_t *)> ff8_worldmap_model_store_depths;   // FF8Recomp: a game function'),
    ('ff8/widescreen.cpp', 'reinterpret_cast<uint32_t>(', 'ffnx_va(', 'all'),

    # ---- world/wmset.cpp: size_t is 64-bit here
    ('ff8/world/wmset.cpp', 'max = std::min(max, pos / 4);', 'max = std::min<size_t>(max, pos / 4);', 'all'),
    ('ff8/world/wmset.cpp', 'max2 = std::min(max2, frame_pos / 4);', 'max2 = std::min<size_t>(max2, frame_pos / 4);'),
]
