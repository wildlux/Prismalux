#pragma once
/* Compatibilità Android NDK: posix_madvise non è disponibile, usa madvise. */
#ifdef __ANDROID__
#include <sys/mman.h>
#ifndef POSIX_MADV_NORMAL
#  define POSIX_MADV_NORMAL    0
#  define POSIX_MADV_RANDOM    1
#  define POSIX_MADV_SEQUENTIAL 2
#  define POSIX_MADV_WILLNEED  3
#  define POSIX_MADV_DONTNEED  4
#endif
static inline int posix_madvise(void* addr, size_t len, int advice) {
    return madvise(addr, len, advice);
}
#endif
