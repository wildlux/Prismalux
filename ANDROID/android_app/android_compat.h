#pragma once
/* Compatibilità Android NDK — posix_madvise.
   NDK 26.1 con minSdk 26 (API 26 ≥ 23) include già posix_madvise
   in sys/mman.h ( __INTRODUCED_IN(23) ).
   Il wrapper inline non serve più — includiamo solo l'header. */
#ifdef __ANDROID__
#include <sys/mman.h>
#ifndef POSIX_MADV_NORMAL
/* Fallback per NDK molto datati (< API 23) — non raggiunto con minSdk 26 */
#  define POSIX_MADV_NORMAL     0
#  define POSIX_MADV_RANDOM     1
#  define POSIX_MADV_SEQUENTIAL 2
#  define POSIX_MADV_WILLNEED   3
#  define POSIX_MADV_DONTNEED   4
static inline int posix_madvise(void* addr, size_t len, int advice) {
    return madvise(addr, len, advice);
}
#endif
#endif
