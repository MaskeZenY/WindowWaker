// SPDX-License-Identifier: MIT
// Minimal strings.h compatibility shim for Windows
#ifndef COS_PC_COMPAT_STRINGS_H
#define COS_PC_COMPAT_STRINGS_H

#if !defined(_WIN32)
#include_next <strings.h>
#else

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __COS_STRCASECMP_DEFINED
#define __COS_STRCASECMP_DEFINED
static inline int strcasecmp(const char* s1, const char* s2) {
    return _stricmp(s1, s2);
}

static inline int strncasecmp(const char* s1, const char* s2, size_t n) {
    return _strnicmp(s1, s2, n);
}
#endif

#ifdef __cplusplus
}
#endif

#endif // _WIN32
#endif // COS_PC_COMPAT_STRINGS_H
