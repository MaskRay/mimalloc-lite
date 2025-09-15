/* ----------------------------------------------------------------------------
Copyright (c) 2018-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

// ------------------------------------------------------------------------
// mi prefixed publi definitions of various Posix, Unix, and C++ functions
// for convenience and used when overriding these functions.
// ------------------------------------------------------------------------
#include "mimalloc.h"
#include "mimalloc/internal.h"

// ------------------------------------------------------
// Posix & Unix functions definitions
// ------------------------------------------------------

#include <errno.h>
#include <string.h>  // memset
#include <stdlib.h>  // getenv

#ifdef _MSC_VER
#pragma warning(disable:4996)  // getenv _wgetenv
#endif

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif


size_t mi_malloc_size(const void* p) {
  // if (!mi_is_in_heap_region(p)) return 0;
  return mi_usable_size(p);
}

size_t mi_malloc_usable_size(const void *p) {
  // if (!mi_is_in_heap_region(p)) return 0;
  return mi_usable_size(p);
}

size_t mi_malloc_good_size(size_t size) {
  return mi_good_size(size);
}

int mi_posix_memalign(void** p, size_t alignment, size_t size) {
  // Note: The spec dictates we should not modify `*p` on an error. (issue#27)
  // <http://man7.org/linux/man-pages/man3/posix_memalign.3.html>
  if (p == NULL) return EINVAL;
  if ((alignment % sizeof(void*)) != 0) return EINVAL;                 // natural alignment
  // it is also required that alignment is a power of 2 and > 0; this is checked in `mi_malloc_aligned`
  if (alignment==0 || !_mi_is_power_of_two(alignment)) return EINVAL;  // not a power of 2
  void* q = mi_malloc_aligned(size, alignment);
  if (q==NULL && size != 0) return ENOMEM;
  mi_assert_internal(((uintptr_t)q % alignment) == 0);
  *p = q;
  return 0;
}

void* mi_memalign(size_t alignment, size_t size) {
  void* p = mi_malloc_aligned(size, alignment);
  mi_assert_internal(((uintptr_t)p % alignment) == 0);
  return p;
}

void* mi_aligned_alloc(size_t alignment, size_t size) {
  // C11 requires the size to be an integral multiple of the alignment, see <https://en.cppreference.com/w/c/memory/aligned_alloc>.
  // unfortunately, it turns out quite some programs pass a size that is not an integral multiple so skip this check..
  /* if mi_unlikely((size & (alignment - 1)) != 0) { // C11 requires alignment>0 && integral multiple, see <https://en.cppreference.com/w/c/memory/aligned_alloc>
      #if MI_DEBUG > 0
      _mi_error_message(EOVERFLOW, "(mi_)aligned_alloc requires the size to be an integral multiple of the alignment (size %zu, alignment %zu)\n", size, alignment);
      #endif
      return NULL;
    }
  */
  // C11 also requires alignment to be a power-of-two (and > 0) which is checked in mi_malloc_aligned
  void* p = mi_malloc_aligned(size, alignment);
  mi_assert_internal(((uintptr_t)p % alignment) == 0);
  return p;
}

void* mi_reallocarray( void* p, size_t count, size_t size ) {  // BSD
  void* newp = mi_reallocn(p,count,size);
  if (newp==NULL) { errno = ENOMEM; }
  return newp;
}

int mi_reallocarr( void* p, size_t count, size_t size ) { // NetBSD
  mi_assert(p != NULL);
  if (p == NULL) {
    errno = EINVAL;
    return EINVAL;
  }
  void** op = (void**)p;
  void* newp = mi_reallocarray(*op, count, size);
  if mi_unlikely(newp == NULL) { return errno; }
  *op = newp;
  return 0;
}
