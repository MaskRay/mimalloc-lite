/* ----------------------------------------------------------------------------
Copyright (c) 2018-2024, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE   // for realpath() on Linux
#endif

#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/atomic.h"
#include "mimalloc/prim.h"   // _mi_prim_thread_id()

#include <string.h>      // memset, strlen (for mi_strdup)
#include <stdlib.h>      // malloc, abort

#define MI_IN_ALLOC_C
#include "alloc-override.c"
#include "free.c"
#undef MI_IN_ALLOC_C

// ------------------------------------------------------
// Allocation
// ------------------------------------------------------

// Fast allocation in a page: just pop from the free list.
// Fall back to generic allocation only if the list is empty.
// Note: in release mode the (inlined) routine is about 7 instructions with a single test.
extern inline void* _mi_page_malloc_zero(mi_heap_t* heap, mi_page_t* page, size_t size, bool zero) mi_attr_noexcept
{
  mi_assert_internal(page->block_size == 0 /* empty heap */ || mi_page_block_size(page) >= size);

  // check the free list
  mi_block_t* const block = page->free;
  if mi_unlikely(block == NULL) {
    return _mi_malloc_generic(heap, size, zero, 0);
  }
  mi_assert_internal(block != NULL && _mi_ptr_page(block) == page);

  // pop from the free list
  page->free = mi_block_next(page, block);
  page->used++;
  mi_assert_internal(page->free == NULL || _mi_ptr_page(page->free) == page);
  mi_assert_internal(page->block_size < MI_MAX_ALIGN_SIZE || _mi_is_aligned(block, MI_MAX_ALIGN_SIZE));

  // allow use of the block internally
  mi_track_mem_undefined(block, mi_page_usable_block_size(page));

  // zero the block? note: we need to zero the full block size (issue #63)
  if mi_unlikely(zero) {
    mi_assert_internal(page->block_size != 0); // do not call with zero'ing for huge blocks (see _mi_malloc_generic)
    mi_assert_internal(!mi_page_is_huge(page));
    
    if (page->free_is_zero) {
      block->next = 0;
      mi_track_mem_defined(block, page->block_size);
    }
    else {
      _mi_memzero_aligned(block, page->block_size);
    }
  }

  #if (MI_DEBUG>0) && !MI_TRACK_ENABLED && !MI_TSAN
  if (!zero && !mi_page_is_huge(page)) {
    memset(block, MI_DEBUG_UNINIT, mi_page_usable_block_size(page));
  }
  
  #endif

  return block;
}

static inline void* mi_heap_malloc_small_zero(mi_heap_t* heap, size_t size, bool zero) {
  mi_assert(heap != NULL);
  mi_assert(size <= MI_SMALL_SIZE_MAX);
  #if MI_DEBUG
  const uintptr_t tid = _mi_thread_id();
  mi_assert(heap->thread_id == 0 || heap->thread_id == tid); // heaps are thread local
  #endif


  // get page in constant time, and allocate from it
  mi_page_t* page = _mi_heap_get_free_small_page(heap, size);
  void* const p = _mi_page_malloc_zero(heap, page, size, zero);
  mi_track_malloc(p,size,zero);

  #if MI_DEBUG>3
  if (p != NULL && zero) {
    mi_assert_expensive(mi_mem_is_zero(p, size));
  }
  #endif
  return p;
}

// allocate a small block
extern inline void* mi_heap_malloc_small(mi_heap_t* heap, size_t size) {
  return mi_heap_malloc_small_zero(heap, size, false);
}

// The main allocation function
extern inline void* _mi_heap_malloc_zero_ex(mi_heap_t* heap, size_t size, bool zero, size_t huge_alignment) {
  // fast path for small objects
  if mi_likely(size <= MI_SMALL_SIZE_MAX) {
    mi_assert_internal(huge_alignment == 0);
    return mi_heap_malloc_small_zero(heap, size, zero);
  }

  else {
    // regular allocation
    mi_assert(heap!=NULL);
    mi_assert(heap->thread_id == 0 || heap->thread_id == _mi_thread_id());   // heaps are thread local
    void* const p = _mi_malloc_generic(heap, size, zero, huge_alignment);  // note: size can overflow but it is detected in malloc_generic
    mi_track_malloc(p,size,zero);

    #if MI_DEBUG>3
    if (p != NULL && zero) {
      mi_assert_expensive(mi_mem_is_zero(p, size));
    }
    #endif
    return p;
  }
}

extern inline void* _mi_heap_malloc_zero(mi_heap_t* heap, size_t size, bool zero) {
  return _mi_heap_malloc_zero_ex(heap, size, zero, 0);
}

extern inline void* mi_heap_malloc(mi_heap_t* heap, size_t size) {
  return _mi_heap_malloc_zero(heap, size, false);
}

extern inline void* mi_malloc(size_t size) {
  return mi_heap_malloc(mi_prim_get_default_heap(), size);
}

extern inline void* mi_heap_zalloc(mi_heap_t* heap, size_t size) {
  return _mi_heap_malloc_zero(heap, size, true);
}


extern inline void* mi_heap_calloc(mi_heap_t* heap, size_t count, size_t size) {
  size_t total;
  if (mi_count_size_overflow(count,size,&total)) return NULL;
  return mi_heap_zalloc(heap,total);
}

void* mi_calloc(size_t count, size_t size) {
  return mi_heap_calloc(mi_prim_get_default_heap(),count,size);
}

void* _mi_heap_realloc_zero(mi_heap_t* heap, void* p, size_t newsize, bool zero) {
  // if p == NULL then behave as malloc.
  // else if size == 0 then reallocate to a zero-sized block (and don't return NULL, just as mi_malloc(0)).
  // (this means that returning NULL always indicates an error, and `p` will not have been freed in that case.)
  const size_t size = _mi_usable_size(p,"mi_realloc"); // also works if p == NULL (with size 0)
  if mi_unlikely(newsize <= size && newsize >= (size / 2) && newsize > 0) {  // note: newsize must be > 0 or otherwise we return NULL for realloc(NULL,0)
    mi_assert_internal(p!=NULL);
    // todo: do not track as the usable size is still the same in the free; adjust potential padding?
    // mi_track_resize(p,size,newsize)
    // if (newsize < size) { mi_track_mem_noaccess((uint8_t*)p + newsize, size - newsize); }
    return p;  // reallocation still fits and not more than 50% waste
  }
  void* newp = mi_heap_malloc(heap,newsize);
  if mi_likely(newp != NULL) {
    if (zero && newsize > size) {
      // also set last word in the previous allocation to zero to ensure any padding is zero-initialized
      const size_t start = (size >= sizeof(intptr_t) ? size - sizeof(intptr_t) : 0);
      _mi_memzero((uint8_t*)newp + start, newsize - start);
    }
    else if (newsize == 0) {
      ((uint8_t*)newp)[0] = 0; // work around for applications that expect zero-reallocation to be zero initialized (issue #725)
    }
    if mi_likely(p != NULL) {
      const size_t copysize = (newsize > size ? size : newsize);
      mi_track_mem_defined(p,copysize);  // _mi_useable_size may be too large for byte precise memory tracking..
      memcpy(newp, p, copysize);
      mi_free(p); // only free the original pointer if successful
    }
  }
  return newp;
}

void* mi_heap_realloc(mi_heap_t* heap, void* p, size_t newsize) {
  return _mi_heap_realloc_zero(heap, p, newsize, false);
}

void* mi_heap_reallocn(mi_heap_t* heap, void* p, size_t count, size_t size) {
  size_t total;
  if (mi_count_size_overflow(count, size, &total)) return NULL;
  return mi_heap_realloc(heap, p, total);
}


// Reallocate but free `p` on errors
void* mi_heap_reallocf(mi_heap_t* heap, void* p, size_t newsize) {
  void* newp = mi_heap_realloc(heap, p, newsize);
  if (newp==NULL && p!=NULL) mi_free(p);
  return newp;
}

void* mi_heap_rezalloc(mi_heap_t* heap, void* p, size_t newsize) {
  return _mi_heap_realloc_zero(heap, p, newsize, true);
}


void* mi_realloc(void* p, size_t newsize) {
  return mi_heap_realloc(mi_prim_get_default_heap(),p,newsize);
}

void* mi_reallocn(void* p, size_t count, size_t size) {
  return mi_heap_reallocn(mi_prim_get_default_heap(),p,count,size);
}

void* mi_rezalloc(void* p, size_t newsize) {
  return mi_heap_rezalloc(mi_prim_get_default_heap(), p, newsize);
}
