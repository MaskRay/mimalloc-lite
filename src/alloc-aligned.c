/* ----------------------------------------------------------------------------
Copyright (c) 2018-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/prim.h"  // mi_prim_get_default_heap

#include <string.h>     // memset

// ------------------------------------------------------
// Aligned Allocation
// ------------------------------------------------------

static bool mi_malloc_is_naturally_aligned( size_t size, size_t alignment ) {
  // objects up to `MI_MAX_ALIGN_GUARANTEE` are allocated aligned to their size (see `segment.c:_mi_segment_page_start`).
  mi_assert_internal(_mi_is_power_of_two(alignment) && (alignment > 0));
  if (alignment > size) return false;
  if (alignment <= MI_MAX_ALIGN_SIZE) return true;
  const size_t bsize = mi_good_size(size);
  return (bsize <= MI_MAX_ALIGN_GUARANTEE && (bsize & (alignment-1)) == 0);
}

static void* mi_heap_malloc_zero_no_guarded(mi_heap_t* heap, size_t size, bool zero) {
  return _mi_heap_malloc_zero(heap, size, zero);
}


// Fallback aligned allocation that over-allocates -- split out for better codegen
static mi_decl_noinline void* mi_heap_malloc_zero_aligned_at_overalloc(mi_heap_t* const heap, const size_t size, const size_t alignment, const bool zero) mi_attr_noexcept
{
  mi_assert_internal(size <= MI_MAX_ALLOC_SIZE);
  mi_assert_internal(alignment != 0 && _mi_is_power_of_two(alignment));

  void* p;
  size_t oversize;
  if mi_unlikely(alignment > MI_BLOCK_ALIGNMENT_MAX) {
    // use OS allocation for very large alignment and allocate inside a huge page (dedicated segment with 1 page)
    // This can support alignments >= MI_SEGMENT_SIZE by ensuring the object can be aligned at a point in the
    // first (and single) page such that the segment info is `MI_SEGMENT_SIZE` bytes before it (so it can be found by aligning the pointer down)
    oversize = (size <= MI_SMALL_SIZE_MAX ? MI_SMALL_SIZE_MAX + 1 /* ensure we use generic malloc path */ : size);
    // note: no guarded as alignment > 0
    p = _mi_heap_malloc_zero_ex(heap, oversize, false, alignment); // the page block size should be large enough to align in the single huge page block
    // zero afterwards as only the area from the aligned_p may be committed!
    if (p == NULL) return NULL;
  }
  else {
    // otherwise over-allocate
    oversize = (size < MI_MAX_ALIGN_SIZE ? MI_MAX_ALIGN_SIZE : size) + alignment - 1;  // adjust for size <= 16; with size 0 and aligment 64k, we would allocate a 64k block and pointing just beyond that.
    p = mi_heap_malloc_zero_no_guarded(heap, oversize, zero);
    if (p == NULL) return NULL;
  }
  mi_page_t* page = _mi_ptr_page(p);

  // .. and align within the allocation
  const uintptr_t align_mask = alignment - 1;  // for any x, `(x & align_mask) == (x % alignment)`
  const uintptr_t poffset = (uintptr_t)p & align_mask;
  const uintptr_t adjust  = (poffset == 0 ? 0 : alignment - poffset);
  mi_assert_internal(adjust < alignment);
  void* aligned_p = (void*)((uintptr_t)p + adjust);
  if (aligned_p != p) {
    mi_page_set_has_aligned(page, true);
  }
  // todo: expand padding if overallocated ?

  mi_assert_internal(mi_page_usable_block_size(page) >= adjust + size);
  mi_assert_internal(((uintptr_t)aligned_p) % alignment == 0);
  mi_assert_internal(mi_usable_size(aligned_p)>=size);
  mi_assert_internal(mi_usable_size(p) == mi_usable_size(aligned_p)+adjust);
  #if MI_DEBUG > 1
  mi_page_t* const apage = _mi_ptr_page(aligned_p);
  void* unalign_p = _mi_page_ptr_unalign(apage, aligned_p);
  mi_assert_internal(p == unalign_p);
  #endif

  // now zero the block if needed
  if (alignment > MI_BLOCK_ALIGNMENT_MAX) {
    // for the tracker, on huge aligned allocations only the memory from the start of the large block is defined
    mi_track_mem_undefined(aligned_p, size);
    if (zero) {
      _mi_memzero_aligned(aligned_p, mi_usable_size(aligned_p));
    }
  }

  if (p != aligned_p) {
    mi_track_align(p,aligned_p,adjust,mi_usable_size(aligned_p));

  }
  return aligned_p;
}

// Generic primitive aligned allocation -- split out for better codegen
static mi_decl_noinline void* mi_heap_malloc_zero_aligned_at_generic(mi_heap_t* const heap, const size_t size, const size_t alignment, const bool zero) mi_attr_noexcept
{
  mi_assert_internal(alignment != 0 && _mi_is_power_of_two(alignment));
  // we don't allocate more than MI_MAX_ALLOC_SIZE (see <https://sourceware.org/ml/libc-announce/2019/msg00001.html>)
  if mi_unlikely(size > MI_MAX_ALLOC_SIZE) {
    #if MI_DEBUG > 0
    _mi_error_message(EOVERFLOW, "aligned allocation request is too large (size %zu, alignment %zu)\n", size, alignment);
    #endif
    return NULL;
  }

  // use regular allocation if it is guaranteed to fit the alignment constraints.
  // this is important to try as the fast path in `mi_heap_malloc_zero_aligned` only works when there exist
  // a page with the right block size, and if we always use the over-alloc fallback that would never happen.
  if (mi_malloc_is_naturally_aligned(size,alignment)) {
    void* p = mi_heap_malloc_zero_no_guarded(heap, size, zero);
    mi_assert_internal(p == NULL || ((uintptr_t)p % alignment) == 0);
    const bool is_aligned_or_null = (((uintptr_t)p) & (alignment-1))==0;
    if mi_likely(is_aligned_or_null) {
      return p;
    }
    else {
      // this should never happen if the `mi_malloc_is_naturally_aligned` check is correct..
      mi_assert(false);
      mi_free(p);
    }
  }

  // fall back to over-allocation
  return mi_heap_malloc_zero_aligned_at_overalloc(heap,size,alignment,zero);
}


// Primitive aligned allocation
static void* mi_heap_malloc_zero_aligned_at(mi_heap_t* const heap, const size_t size, const size_t alignment, const bool zero) mi_attr_noexcept
{
  // note: we don't require `size > offset`, we just guarantee that the address at offset is aligned regardless of the allocated size.
  if mi_unlikely(alignment == 0 || !_mi_is_power_of_two(alignment)) { // require power-of-two (see <https://en.cppreference.com/w/c/memory/aligned_alloc>)
    #if MI_DEBUG > 0
    _mi_error_message(EOVERFLOW, "aligned allocation requires the alignment to be a power-of-two (size %zu, alignment %zu)\n", size, alignment);
    #endif
    return NULL;
  }



  // try first if there happens to be a small block available with just the right alignment
  if mi_likely(size <= MI_SMALL_SIZE_MAX && alignment <= size) {
    const uintptr_t align_mask = alignment-1;       // for any x, `(x & align_mask) == (x % alignment)`
    const size_t padsize = size;
    mi_page_t* page = _mi_heap_get_free_small_page(heap, padsize);
    if mi_likely(page->free != NULL) {
      const bool is_aligned = (((uintptr_t)page->free + 0) & align_mask)==0;
      if mi_likely(is_aligned)
      {
        void* p = _mi_page_malloc_zero(heap,page,padsize,true); // call specific page malloc for better codegen
        mi_assert_internal(p != NULL);
        mi_assert_internal(((uintptr_t)p + 0) % alignment == 0);
        mi_track_malloc(p,size,zero);
        return p;
      }
    }
  }

  // fallback to generic aligned allocation
  return mi_heap_malloc_zero_aligned_at_generic(heap, size, alignment, zero);
}

void* mi_heap_malloc_aligned(mi_heap_t* heap, size_t size, size_t alignment) mi_attr_noexcept {
  return mi_heap_malloc_zero_aligned_at(heap, size, alignment, false);
}

void* mi_malloc_aligned(size_t size, size_t alignment) mi_attr_noexcept {
  return mi_heap_malloc_aligned(mi_prim_get_default_heap(), size, alignment);
}
