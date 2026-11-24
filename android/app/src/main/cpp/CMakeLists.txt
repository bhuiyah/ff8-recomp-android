# libff8launcher.so for com.ff8recomp (and, for development, libff8.so: the lifted game, its
# runtime, the FFNx bridge and bgfx).
cmake_minimum_required(VERSION 3.20)
project(FF7App C CXX)
get_filename_component(FF7_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
# no build-machine paths in the binaries (__FILE__ in asserts); tools/env.sh FF7_PREFIX_MAP
add_compile_options("-ffile-prefix-map=${FF7_ROOT}=ff7recomp")

# The APK carries no game code: the device builds libff8.so from the player's own exe
# (BuildService.java, with the kit from tools/build_device_kit.sh). FF7_BUNDLE_GAME (gradle
# -PbundleGame=ON, needs generated/recomp) links the PC's lift into the APK, for development.
option(FF7_BUNDLE_GAME "Link the lifted game into the APK (development)" OFF)
if(FF7_BUNDLE_GAME)
  set(FF7_WITH_BGFX ON CACHE BOOL "" FORCE)
  set(FF7_WITH_FFNX ON CACHE BOOL "" FORCE)
  include("${FF7_ROOT}/cmake/ff8core.cmake")
  add_library(ff8 SHARED "${FF7_ROOT}/src/android/ff8_android_main.cpp")
  target_link_libraries(ff8 PRIVATE ff8_core android log)
endif()

# libff8launcher.so for the launcher activity: mod archives and mod.xml read with the same
# code as the game's mod overlay (src/mods), without loading the game.
set(FF7_DEPS "${FF7_ROOT}/external/deps")
add_library(ff8launcher SHARED
  "${FF7_ROOT}/src/android/ff8_launcher_jni.cpp"
  "${FF7_ROOT}/src/mods/lzs.c"
  "${FF7_ROOT}/src/mods/iro.cpp"
  "${FF7_ROOT}/src/mods/modinfo.cpp"
  "${FF7_ROOT}/src/mods/mods.cpp"
  "${FF7_ROOT}/src/mods/runtimevar.cpp"
  "${FF7_DEPS}/lzma/LzmaDec.c"
  "${FF7_DEPS}/pugixml/src/pugixml.cpp"
  # .7z mod downloads (src/android/un7z.c)
  "${FF7_ROOT}/src/android/un7z.c"
  "${FF7_DEPS}/lzma/7zArcIn.c"
  "${FF7_DEPS}/lzma/7zBuf.c"
  "${FF7_DEPS}/lzma/7zCrc.c"
  "${FF7_DEPS}/lzma/7zCrcOpt.c"
  "${FF7_DEPS}/lzma/7zDec.c"
  "${FF7_DEPS}/lzma/7zStream.c"
  "${FF7_DEPS}/lzma/7zAlloc.c"
  "${FF7_DEPS}/lzma/7zFile.c"
  "${FF7_DEPS}/lzma/Bcj2.c"
  "${FF7_DEPS}/lzma/Bra.c"
  "${FF7_DEPS}/lzma/Bra86.c"
  "${FF7_DEPS}/lzma/BraIA64.c"
  "${FF7_DEPS}/lzma/CpuArch.c"
  "${FF7_DEPS}/lzma/Delta.c"
  "${FF7_DEPS}/lzma/Lzma2Dec.c")
target_include_directories(ff8launcher PRIVATE "${FF7_ROOT}/src/mods" "${FF7_DEPS}/lzma" "${FF7_DEPS}/pugixml/src")
target_compile_options(ff8launcher PRIVATE -O2)
target_compile_features(ff8launcher PRIVATE cxx_std_20)
target_link_libraries(ff8launcher PRIVATE log)
