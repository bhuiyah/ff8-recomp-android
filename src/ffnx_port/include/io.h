// io.h - MSVC's POSIX-ish file API names on Android.
// Functions, not macros: FF8's address table (ff8.h ff8_externals) has members named _open,
// _read, _write, _lseek, _close, which a macro would rename wherever this header comes later.
//
// The _O_* flags keep MSVC's values: FFNx's FF8 code builds them for the *game's* CRT _open
// (ff8/file.cpp ff8_open_file), which reads them as Windows flags. With _O_BINARY = 0 the
// game's CRT opened its archives in text mode and stopped at the first Ctrl-Z (menu.fi has
// one at offset 217). The host-side functions below translate them to POSIX.
#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef _access
#define _access access
#endif
#define _O_RDONLY 0x0000
#define _O_WRONLY 0x0001
#define _O_RDWR   0x0002
#define _O_APPEND 0x0008
#define _O_CREAT  0x0100
#define _O_TRUNC  0x0200
#define _O_EXCL   0x0400
#define _O_TEXT   0x4000
#define _O_BINARY 0x8000
static inline int msvc_oflag_to_posix(int f) {
    int p = (f & _O_RDWR) ? O_RDWR : (f & _O_WRONLY) ? O_WRONLY : O_RDONLY;
    if (f & _O_APPEND) p |= O_APPEND;
    if (f & _O_CREAT)  p |= O_CREAT;
    if (f & _O_TRUNC)  p |= O_TRUNC;
    if (f & _O_EXCL)   p |= O_EXCL;
    return p;   // _O_TEXT / _O_BINARY: no such thing on POSIX
}
static inline int _open(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & _O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap); }
    return open(path, msvc_oflag_to_posix(flags), mode);
}
static inline int _close(int fd) { return close(fd); }
static inline int _read(int fd, void *buf, unsigned int n) { return (int)read(fd, buf, n); }
static inline int _write(int fd, const void *buf, unsigned int n) { return (int)write(fd, buf, n); }
static inline long _lseek(int fd, long off, int whence) { return (long)lseek(fd, off, whence); }
static inline long _filelength(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0 ? (long)st.st_size : -1L;
}
// _sopen_s (MSVC): open with a share mode, which POSIX has no notion of.
static inline int _sopen_s_impl(int *fd, const char *path, int flags, int share, int mode) {
    (void)share;
    *fd = open(path, msvc_oflag_to_posix(flags), mode);
    return *fd < 0 ? errno : 0;
}
#define _sopen_s _sopen_s_impl
static inline char *itoa(int value, char *buf, int radix) {
    if (radix == 16) sprintf(buf, "%x", value);
    else if (radix == 8) sprintf(buf, "%o", value);
    else sprintf(buf, "%d", value);
    return buf;
}
