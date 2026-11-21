// io.h - MSVC's POSIX-ish file API names, mapped to the real POSIX ones.
// Functions, not macros: FF8's address table (ff8.h ff8_externals) has members named _open,
// _read, _write, _lseek, _close, which a macro would rename wherever this header comes later.
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
static inline int _open(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap); }
    return open(path, flags, mode);
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
    *fd = open(path, flags, mode);
    return *fd < 0 ? errno : 0;
}
#define _sopen_s _sopen_s_impl
static inline char *itoa(int value, char *buf, int radix) {
    if (radix == 16) sprintf(buf, "%x", value);
    else if (radix == 8) sprintf(buf, "%o", value);
    else sprintf(buf, "%d", value);
    return buf;
}
#define _O_RDONLY O_RDONLY
#define _O_WRONLY O_WRONLY
#define _O_RDWR   O_RDWR
#define _O_CREAT  O_CREAT
#define _O_TRUNC  O_TRUNC
#define _O_APPEND O_APPEND
#define _O_EXCL   O_EXCL
#define _O_BINARY 0
#define _O_TEXT   0
