// SPDX-License-Identifier: MIT
// Minimal unistd.h compatibility shim for Windows
#ifndef COS_PC_COMPAT_UNISTD_H
#define COS_PC_COMPAT_UNISTD_H

#if !defined(_WIN32)
#include_next <unistd.h>
#else

#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef int64_t off_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Sleep from kernel32 without pulling in windows.h
void __stdcall Sleep(unsigned long dwMilliseconds);

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#endif

#ifdef _WIN32
#define stat _stat64
#endif

#include <fcntl.h>
#include <stdarg.h>

#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY _O_WRONLY
#endif
#ifndef O_RDWR
#define O_RDWR _O_RDWR
#endif
#ifndef O_APPEND
#define O_APPEND _O_APPEND
#endif
#ifndef O_CREAT
#define O_CREAT _O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC _O_TRUNC
#endif
#ifndef O_EXCL
#define O_EXCL _O_EXCL
#endif
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif

#ifndef O_CLOEXEC
#ifdef _O_NOINHERIT
#define O_CLOEXEC _O_NOINHERIT
#else
#define O_CLOEXEC 0
#endif
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static inline int open(const char* path, int oflag, ...) {
    int mode = 0;
    if (oflag & _O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return _open(path, oflag, mode);
}

#if defined(_WIN32)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

typedef uint32_t useconds_t;

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define R_OK 4
#define W_OK 2
#define X_OK 0
#define F_OK 0

#include <direct.h>

static inline int access(const char* path, int mode) {
    return _access(path, mode);
}

static inline int mkdir(const char* path, int mode) {
    (void)mode;
    return _mkdir(path);
}

static inline int unlink(const char* path) {
    return _unlink(path);
}

static inline int isatty(int fd) {
    return _isatty(fd);
}

static inline int fileno(FILE* stream) {
    return _fileno(stream);
}

static inline int getpid(void) {
    return _getpid();
}

static inline int close(int fd) {
    return _close(fd);
}

static inline int read(int fd, void* buf, unsigned int count) {
    return _read(fd, buf, count);
}

static inline int write(int fd, const void* buf, unsigned int count) {
    return _write(fd, buf, count);
}

static inline ssize_t pread(int fd, void* buf, size_t count, int64_t offset) {
    int64_t cur = _lseeki64(fd, 0, SEEK_CUR);
    if (_lseeki64(fd, offset, SEEK_SET) < 0) return -1;
    int bytes = _read(fd, buf, (unsigned int)count);
    _lseeki64(fd, cur, SEEK_SET);
    return (ssize_t)bytes;
}

static inline ssize_t pwrite(int fd, const void* buf, size_t count, int64_t offset) {
    int64_t cur = _lseeki64(fd, 0, SEEK_CUR);
    if (_lseeki64(fd, offset, SEEK_SET) < 0) return -1;
    int bytes = _write(fd, buf, (unsigned int)count);
    _lseeki64(fd, cur, SEEK_SET);
    return (ssize_t)bytes;
}

static inline unsigned int sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
    return 0;
}

#include <time.h>

static inline int usleep(useconds_t usec) {
    Sleep(usec / 1000);
    return 0;
}

static inline int nanosleep(const struct timespec* req, struct timespec* rem) {
    (void)rem;
    if (!req) return -1;
    unsigned long ms = (unsigned long)(req->tv_sec * 1000 + (req->tv_nsec + 999999) / 1000000);
    Sleep(ms);
    return 0;
}

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

static inline int clock_gettime(int clk_id, struct timespec* ts) {
    (void)clk_id;
    if (!ts) return -1;
    return timespec_get(ts, TIME_UTC) ? 0 : -1;
}

static inline int pause(void) {
    Sleep(0xFFFFFFFF);
    return -1;
}

static inline int ftrylockfile(FILE* f) {
    _lock_file(f);
    return 0;
}

static inline void funlockfile(FILE* f) {
    _unlock_file(f);
}

static inline void flockfile(FILE* f) {
    _lock_file(f);
}

static inline int fseeko(FILE* stream, int64_t offset, int origin) {
    return _fseeki64(stream, offset, origin);
}

static inline int64_t ftello(FILE* stream) {
    return _ftelli64(stream);
}

static inline int setenv(const char* name, const char* value, int overwrite) {
    if (!overwrite && getenv(name) != NULL) {
        return 0;
    }
    return _putenv_s(name, value);
}

static inline int unsetenv(const char* name) {
    return _putenv_s(name, "");
}

static inline char* realpath(const char* path, char* resolved_path) {
    return _fullpath(resolved_path, path, PATH_MAX);
}

#include <strings.h>

#ifdef __cplusplus
}
#endif

#ifdef EXCEPTION_BREAKPOINT
#undef EXCEPTION_BREAKPOINT
#endif

#endif // _WIN32
#endif // COS_PC_COMPAT_UNISTD_H
