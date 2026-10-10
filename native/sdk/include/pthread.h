// SPDX-License-Identifier: MIT
// Minimal POSIX thread compatibility shim for Windows
#ifndef COS_SDK_PTHREAD_COMPAT_H
#define COS_SDK_PTHREAD_COMPAT_H

#if !defined(_WIN32)
#include_next <pthread.h>
#else

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t pthread_t;

typedef struct {
    size_t stack_size;
    int detach_state;
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

static inline int pthread_attr_init(pthread_attr_t* attr) {
    if (!attr) return -1;
    attr->stack_size = 0;
    attr->detach_state = PTHREAD_CREATE_JOINABLE;
    return 0;
}

static inline int pthread_attr_destroy(pthread_attr_t* attr) {
    (void)attr;
    return 0;
}

static inline int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize) {
    if (!attr) return -1;
    attr->stack_size = stacksize;
    return 0;
}

static inline int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate) {
    if (!attr) return -1;
    attr->detach_state = detachstate;
    return 0;
}

struct __cos_pthread_thunk {
    void* (*func)(void*);
    void* arg;
    int detached;
};

static inline unsigned __stdcall __cos_pthread_entry(void* p) {
    struct __cos_pthread_thunk thunk = *(struct __cos_pthread_thunk*)p;
    free(p);
    void* ret = thunk.func(thunk.arg);
    return (unsigned)(uintptr_t)ret;
}

static inline int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine)(void*), void* arg) {
    struct __cos_pthread_thunk* thunk = (struct __cos_pthread_thunk*)malloc(sizeof(struct __cos_pthread_thunk));
    if (!thunk) return -1;
    thunk->func = start_routine;
    thunk->arg = arg;
    thunk->detached = (attr && attr->detach_state == PTHREAD_CREATE_DETACHED);
    size_t stack = attr ? attr->stack_size : 0;
    uintptr_t handle = _beginthreadex(NULL, (unsigned)stack, __cos_pthread_entry, thunk, 0, NULL);
    if (handle == 0) {
        free(thunk);
        return -1;
    }
    if (thunk->detached) {
        CloseHandle((HANDLE)handle);
        if (thread) *thread = 0;
    } else {
        if (thread) *thread = handle;
    }
    return 0;
}

static inline int pthread_detach(pthread_t thread) {
    if (thread != 0) {
        CloseHandle((HANDLE)thread);
    }
    return 0;
}

static inline int pthread_join(pthread_t thread, void** retval) {
    if (thread == 0) return -1;
    WaitForSingleObject((HANDLE)thread, INFINITE);
    if (retval) {
        DWORD exitCode = 0;
        GetExitCodeThread((HANDLE)thread, &exitCode);
        *retval = (void*)(uintptr_t)exitCode;
    }
    CloseHandle((HANDLE)thread);
    return 0;
}

#ifdef __cplusplus
[[noreturn]]
#endif
static inline void pthread_exit(void* retval) {
    _endthreadex((unsigned)(uintptr_t)retval);
    for (;;) { ExitThread(0); }
}

static inline pthread_t pthread_self(void) {
    return (pthread_t)GetCurrentThread();
}

static inline int pthread_getname_np(pthread_t thread, char* buf, size_t buflen) {
    (void)thread;
    if (buf && buflen > 0) buf[0] = '\0';
    return 0;
}

#ifdef __cplusplus
}
static inline int pthread_setname_np(const char*) { return 0; }
static inline int pthread_setname_np(pthread_t, const char*) { return 0; }
#endif

#endif // _WIN32
#endif // COS_SDK_PTHREAD_COMPAT_H
