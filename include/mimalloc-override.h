/* ----------------------------------------------------------------------------
Copyright (c) 2018-2020 Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once

/* ----------------------------------------------------------------------------
This header can be used to statically redirect malloc/free and new/delete
to the mimalloc variants. This can be useful if one can include this file on
each source file in a project (but be careful when using external code to
not accidentally mix pointers from different allocators).
-----------------------------------------------------------------------------*/

#include <mimalloc.h>

// Standard C allocation
#define malloc(n)               mi_malloc(n)
#define calloc(n,c)             mi_calloc(n,c)
#define realloc(p,n)            mi_realloc(p,n)
#define free(p)                 mi_free(p)

// Various Posix and Unix variants
#define malloc_size(p)          mi_usable_size(p)
#define malloc_usable_size(p)   mi_usable_size(p)
#define malloc_good_size(sz)    mi_malloc_good_size(sz)

#define reallocarray(p,s,n)     mi_reallocarray(p,s,n)
#define reallocarr(p,s,n)       mi_reallocarr(p,s,n)
#define memalign(a,n)           mi_memalign(a,n)
#define aligned_alloc(a,n)      mi_aligned_alloc(a,n)
#define posix_memalign(p,a,n)   mi_posix_memalign(p,a,n)
#define _posix_memalign(p,a,n)  mi_posix_memalign(p,a,n)
