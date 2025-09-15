/* ----------------------------------------------------------------------------
Copyright (c) 2018-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

#if !defined(MI_IN_ALLOC_C)
#error "this file should be included from 'alloc.c' (so aliases can work)"
#endif

#if defined(MI_MALLOC_OVERRIDE)

// helper definition for C override of C++ new
typedef void* mi_nothrow_t;

// ------------------------------------------------------
// Override system malloc
// ------------------------------------------------------

#if !MI_TRACK_ENABLED
  // gcc: use aliasing to alias the exported function to one of our `mi_` functions
  #pragma GCC diagnostic ignored "-Wattributes"  // or we get warnings that nodiscard is ignored on a forward
  #define MI_FORWARD(fun)      __attribute__((alias(#fun), used, visibility("default"), copy(fun)));
  #define MI_FORWARD1(fun,x)      MI_FORWARD(fun)
  #define MI_FORWARD2(fun,x,y)    MI_FORWARD(fun)
  #define MI_FORWARD3(fun,x,y,z)  MI_FORWARD(fun)
  #define MI_FORWARD0(fun,x)      MI_FORWARD(fun)
  #define MI_FORWARD02(fun,x,y)   MI_FORWARD(fun)
#else
  // otherwise use forwarding by calling our `mi_` function
  #define MI_FORWARD1(fun,x)      { return fun(x); }
  #define MI_FORWARD2(fun,x,y)    { return fun(x,y); }
  #define MI_FORWARD3(fun,x,y,z)  { return fun(x,y,z); }
  #define MI_FORWARD0(fun,x)      { fun(x); }
  #define MI_FORWARD02(fun,x,y)   { fun(x,y); }
#endif


  // On all other systems forward allocation primitives to our API
  mi_decl_export void* malloc(size_t size)              MI_FORWARD1(mi_malloc, size)
  mi_decl_export void* calloc(size_t size, size_t n)    MI_FORWARD2(mi_calloc, size, n)
  mi_decl_export void* realloc(void* p, size_t newsize) MI_FORWARD2(mi_realloc, p, newsize)
  mi_decl_export void  free(void* p)                    MI_FORWARD0(mi_free, p)  

#pragma GCC visibility push(default)

// ------------------------------------------------------
// Further Posix & Unix functions definitions
// ------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

  // Forward Posix/Unix calls as well
  size_t malloc_size(const void* p)        MI_FORWARD1(mi_usable_size,p)
  #if !defined(__ANDROID__) && !defined(__FreeBSD__) && !defined(__DragonFly__)
  size_t malloc_usable_size(void *p)       MI_FORWARD1(mi_usable_size,p)
  #else
  size_t malloc_usable_size(const void *p) MI_FORWARD1(mi_usable_size,p)
  #endif

  // No forwarding here due to aliasing/name mangling issues
  size_t malloc_good_size(size_t size)     { return mi_malloc_good_size(size); }
  int    posix_memalign(void** p, size_t alignment, size_t size) { return mi_posix_memalign(p, alignment, size); }

  // `aligned_alloc` is only available when __USE_ISOC11 is defined.
  // Note: it seems __USE_ISOC11 is not defined in musl (and perhaps other libc's) so we only check
  // for it if using glibc.
  // Note: Conda has a custom glibc where `aligned_alloc` is declared `static inline` and we cannot
  // override it, but both _ISOC11_SOURCE and __USE_ISOC11 are undefined in Conda GCC7 or GCC9.
  // Fortunately, in the case where `aligned_alloc` is declared as `static inline` it
  // uses internally `memalign`, `posix_memalign`, or `_aligned_malloc` so we  can avoid overriding it ourselves.
  #if !defined(__GLIBC__) || __USE_ISOC11
  void* aligned_alloc(size_t alignment, size_t size) { return mi_aligned_alloc(alignment, size); }
  #endif

// no forwarding here due to aliasing/name mangling issues
void* memalign(size_t alignment, size_t size)           { return mi_memalign(alignment, size); }
void* _aligned_malloc(size_t alignment, size_t size)    { return mi_aligned_alloc(alignment, size); }
void* reallocarray(void* p, size_t count, size_t size)  { return mi_reallocarray(p, count, size); }
// some systems define reallocarr so mark it as a weak symbol (#751)
mi_decl_weak int reallocarr(void* p, size_t count, size_t size)    { return mi_reallocarr(p, count, size); }

#if defined(__linux__)
  // forward __libc interface (needed for glibc-based and musl-based Linux distributions)
  void* __libc_malloc(size_t size)                      MI_FORWARD1(mi_malloc,size)
  void* __libc_calloc(size_t count, size_t size)        MI_FORWARD2(mi_calloc,count,size)
  void* __libc_realloc(void* p, size_t size)            MI_FORWARD2(mi_realloc,p,size)
  void  __libc_free(void* p)                            MI_FORWARD0(mi_free,p)

  void* __libc_memalign(size_t alignment, size_t size)  { return mi_memalign(alignment,size); }
  int   __posix_memalign(void** p, size_t alignment, size_t size) { return mi_posix_memalign(p,alignment,size); }
#endif

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif // MI_MALLOC_OVERRIDE
