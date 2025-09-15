/* ----------------------------------------------------------------------------
Copyright (c) 2018-2024 Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef MIMALLOC_ATOMIC_H
#define MIMALLOC_ATOMIC_H

// include pthreads.h
#define  MI_USE_PTHREADS
#include <pthread.h>

// --------------------------------------------------------------------------------------------
// Atomics
// We need to be portable between C, C++, and MSVC.
// We base the primitives on the C/C++ atomics and create a minimal wrapper for MSVC in C compilation mode.
// This is why we try to use only `uintptr_t` and `<type>*` as atomic types.
// To gain better insight in the range of used atomics, we use explicitly named memory order operations
// instead of passing the memory order as a parameter.
// -----------------------------------------------------------------------------------------------

#if defined(__cplusplus)
// Use C++ atomics
#include <atomic>
#define  _Atomic(tp)              std::atomic<tp>
#define  mi_atomic(name)          std::atomic_##name
#define  mi_memory_order(name)    std::memory_order_##name
#if (__cplusplus >= 202002L)      // c++20, see issue #571
 #define MI_ATOMIC_VAR_INIT(x)    x
#elif !defined(ATOMIC_VAR_INIT)
 #define MI_ATOMIC_VAR_INIT(x)    x
#else
 #define MI_ATOMIC_VAR_INIT(x)    ATOMIC_VAR_INIT(x)
#endif
#else
// Use C11 atomics
#include <stdatomic.h>
#define  mi_atomic(name)          atomic_##name
#define  mi_memory_order(name)    memory_order_##name
#if (__STDC_VERSION__ >= 201710L) // c17, see issue #735
 #define MI_ATOMIC_VAR_INIT(x)    x
#elif !defined(ATOMIC_VAR_INIT)
 #define MI_ATOMIC_VAR_INIT(x)    x
#else
 #define MI_ATOMIC_VAR_INIT(x)    ATOMIC_VAR_INIT(x)
#endif
#endif

// Various defines for all used memory orders in mimalloc
#define mi_atomic_cas_weak(p,expected,desired,mem_success,mem_fail)  \
  mi_atomic(compare_exchange_weak_explicit)(p,expected,desired,mem_success,mem_fail)

#define mi_atomic_cas_strong(p,expected,desired,mem_success,mem_fail)  \
  mi_atomic(compare_exchange_strong_explicit)(p,expected,desired,mem_success,mem_fail)

#define mi_atomic_load_acquire(p)                mi_atomic(load_explicit)(p,mi_memory_order(acquire))
#define mi_atomic_load_relaxed(p)                mi_atomic(load_explicit)(p,mi_memory_order(relaxed))
#define mi_atomic_store_release(p,x)             mi_atomic(store_explicit)(p,x,mi_memory_order(release))
#define mi_atomic_store_relaxed(p,x)             mi_atomic(store_explicit)(p,x,mi_memory_order(relaxed))
#define mi_atomic_exchange_relaxed(p,x)          mi_atomic(exchange_explicit)(p,x,mi_memory_order(relaxed))
#define mi_atomic_exchange_release(p,x)          mi_atomic(exchange_explicit)(p,x,mi_memory_order(release))
#define mi_atomic_exchange_acq_rel(p,x)          mi_atomic(exchange_explicit)(p,x,mi_memory_order(acq_rel))
#define mi_atomic_cas_weak_release(p,exp,des)    mi_atomic_cas_weak(p,exp,des,mi_memory_order(release),mi_memory_order(relaxed))
#define mi_atomic_cas_weak_acq_rel(p,exp,des)    mi_atomic_cas_weak(p,exp,des,mi_memory_order(acq_rel),mi_memory_order(acquire))
#define mi_atomic_cas_strong_release(p,exp,des)  mi_atomic_cas_strong(p,exp,des,mi_memory_order(release),mi_memory_order(relaxed))
#define mi_atomic_cas_strong_acq_rel(p,exp,des)  mi_atomic_cas_strong(p,exp,des,mi_memory_order(acq_rel),mi_memory_order(acquire))

#define mi_atomic_add_relaxed(p,x)               mi_atomic(fetch_add_explicit)(p,x,mi_memory_order(relaxed))
#define mi_atomic_sub_relaxed(p,x)               mi_atomic(fetch_sub_explicit)(p,x,mi_memory_order(relaxed))
#define mi_atomic_add_acq_rel(p,x)               mi_atomic(fetch_add_explicit)(p,x,mi_memory_order(acq_rel))
#define mi_atomic_sub_acq_rel(p,x)               mi_atomic(fetch_sub_explicit)(p,x,mi_memory_order(acq_rel))
#define mi_atomic_and_acq_rel(p,x)               mi_atomic(fetch_and_explicit)(p,x,mi_memory_order(acq_rel))
#define mi_atomic_or_acq_rel(p,x)                mi_atomic(fetch_or_explicit)(p,x,mi_memory_order(acq_rel))

#define mi_atomic_increment_relaxed(p)           mi_atomic_add_relaxed(p,(uintptr_t)1)
#define mi_atomic_decrement_relaxed(p)           mi_atomic_sub_relaxed(p,(uintptr_t)1)
#define mi_atomic_increment_acq_rel(p)           mi_atomic_add_acq_rel(p,(uintptr_t)1)
#define mi_atomic_decrement_acq_rel(p)           mi_atomic_sub_acq_rel(p,(uintptr_t)1)

static inline void mi_atomic_yield(void);
static inline intptr_t mi_atomic_addi(_Atomic(intptr_t)*p, intptr_t add);
static inline intptr_t mi_atomic_subi(_Atomic(intptr_t)*p, intptr_t sub);


// In C++/C11 atomics we have polymorphic atomics so can use the typed `ptr` variants (where `tp` is the type of atomic value)
#define mi_atomic_load_ptr_acquire(tp,p)                mi_atomic_load_acquire(p)
#define mi_atomic_load_ptr_relaxed(tp,p)                mi_atomic_load_relaxed(p)

// In C++ we need to add casts to help resolve templates if NULL is passed
#if defined(__cplusplus)
#define mi_atomic_store_ptr_release(tp,p,x)             mi_atomic_store_release(p,(tp*)x)
#define mi_atomic_store_ptr_relaxed(tp,p,x)             mi_atomic_store_relaxed(p,(tp*)x)
#define mi_atomic_cas_ptr_weak_release(tp,p,exp,des)    mi_atomic_cas_weak_release(p,exp,(tp*)des)
#define mi_atomic_cas_ptr_weak_acq_rel(tp,p,exp,des)    mi_atomic_cas_weak_acq_rel(p,exp,(tp*)des)
#define mi_atomic_cas_ptr_strong_release(tp,p,exp,des)  mi_atomic_cas_strong_release(p,exp,(tp*)des)
#define mi_atomic_cas_ptr_strong_acq_rel(tp,p,exp,des)  mi_atomic_cas_strong_acq_rel(p,exp,(tp*)des)
#define mi_atomic_exchange_ptr_relaxed(tp,p,x)          mi_atomic_exchange_relaxed(p,(tp*)x)
#define mi_atomic_exchange_ptr_release(tp,p,x)          mi_atomic_exchange_release(p,(tp*)x)
#define mi_atomic_exchange_ptr_acq_rel(tp,p,x)          mi_atomic_exchange_acq_rel(p,(tp*)x)
#else
#define mi_atomic_store_ptr_release(tp,p,x)             mi_atomic_store_release(p,x)
#define mi_atomic_store_ptr_relaxed(tp,p,x)             mi_atomic_store_relaxed(p,x)
#define mi_atomic_cas_ptr_weak_release(tp,p,exp,des)    mi_atomic_cas_weak_release(p,exp,des)
#define mi_atomic_cas_ptr_weak_acq_rel(tp,p,exp,des)    mi_atomic_cas_weak_acq_rel(p,exp,des)
#define mi_atomic_cas_ptr_strong_release(tp,p,exp,des)  mi_atomic_cas_strong_release(p,exp,des)
#define mi_atomic_cas_ptr_strong_acq_rel(tp,p,exp,des)  mi_atomic_cas_strong_acq_rel(p,exp,des)
#define mi_atomic_exchange_ptr_relaxed(tp,p,x)          mi_atomic_exchange_relaxed(p,x)
#define mi_atomic_exchange_ptr_release(tp,p,x)          mi_atomic_exchange_release(p,x)
#define mi_atomic_exchange_ptr_acq_rel(tp,p,x)          mi_atomic_exchange_acq_rel(p,x)
#endif

// These are used by the statistics
static inline int64_t mi_atomic_addi64_relaxed(volatile int64_t* p, int64_t add) {
  return mi_atomic(fetch_add_explicit)((_Atomic(int64_t)*)p, add, mi_memory_order(relaxed));
}
static inline void mi_atomic_void_addi64_relaxed(volatile int64_t* p, const volatile int64_t* padd) {
  const int64_t add = mi_atomic_load_relaxed((_Atomic(int64_t)*)padd);
  if (add != 0) {
    mi_atomic(fetch_add_explicit)((_Atomic(int64_t)*)p, add, mi_memory_order(relaxed));
  }
}
static inline void mi_atomic_maxi64_relaxed(volatile int64_t* p, int64_t x) {
  int64_t current = mi_atomic_load_relaxed((_Atomic(int64_t)*)p);
  while (current < x && !mi_atomic_cas_weak_release((_Atomic(int64_t)*)p, &current, x)) { /* nothing */ };
}

// Used by timers
#define mi_atomic_loadi64_acquire(p)            mi_atomic(load_explicit)(p,mi_memory_order(acquire))
#define mi_atomic_loadi64_relaxed(p)            mi_atomic(load_explicit)(p,mi_memory_order(relaxed))
#define mi_atomic_storei64_release(p,x)         mi_atomic(store_explicit)(p,x,mi_memory_order(release))
#define mi_atomic_storei64_relaxed(p,x)         mi_atomic(store_explicit)(p,x,mi_memory_order(relaxed))

#define mi_atomic_casi64_strong_acq_rel(p,e,d)  mi_atomic_cas_strong_acq_rel(p,e,d)
#define mi_atomic_addi64_acq_rel(p,i)           mi_atomic_add_acq_rel(p,i)




// Atomically add a signed value; returns the previous value.
static inline intptr_t mi_atomic_addi(_Atomic(intptr_t)*p, intptr_t add) {
  return (intptr_t)mi_atomic_add_acq_rel((_Atomic(uintptr_t)*)p, (uintptr_t)add);
}

// Atomically subtract a signed value; returns the previous value.
static inline intptr_t mi_atomic_subi(_Atomic(intptr_t)*p, intptr_t sub) {
  return (intptr_t)mi_atomic_addi(p, -sub);
}


// ----------------------------------------------------------------------
// Once and Guard
// ----------------------------------------------------------------------

typedef _Atomic(uintptr_t) mi_atomic_once_t;

// Returns true only on the first invocation
static inline bool mi_atomic_once( mi_atomic_once_t* once ) {
  if (mi_atomic_load_relaxed(once) != 0) return false;     // quick test
  uintptr_t expected = 0;
  return mi_atomic_cas_strong_acq_rel(once, &expected, (uintptr_t)1); // try to set to 1
}

typedef _Atomic(uintptr_t) mi_atomic_guard_t;

// Allows only one thread to execute at a time
#define mi_atomic_guard(guard) \
  uintptr_t _mi_guard_expected = 0; \
  for(bool _mi_guard_once = true; \
      _mi_guard_once && mi_atomic_cas_strong_acq_rel(guard,&_mi_guard_expected,(uintptr_t)1); \
      (mi_atomic_store_release(guard,(uintptr_t)0), _mi_guard_once = false) )



// ----------------------------------------------------------------------
// Yield
// ----------------------------------------------------------------------

#if defined(__cplusplus)
#include <thread>
static inline void mi_atomic_yield(void) {
  std::this_thread::yield();
}
#elif defined(__SSE2__)
#include <emmintrin.h>
static inline void mi_atomic_yield(void) {
  _mm_pause();
}
#elif (defined(__GNUC__) || defined(__clang__)) && \
      (defined(__x86_64__) || defined(__i386__) || \
       defined(__aarch64__) || defined(__arm__) || \
       defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(__POWERPC__))
#if defined(__x86_64__) || defined(__i386__)
static inline void mi_atomic_yield(void) {
  __asm__ volatile ("pause" ::: "memory");
}
#elif defined(__aarch64__)
static inline void mi_atomic_yield(void) {
  __asm__ volatile("wfe");
}
#elif defined(__arm__)
#if __ARM_ARCH >= 7
static inline void mi_atomic_yield(void) {
  __asm__ volatile("yield" ::: "memory");
}
#else
static inline void mi_atomic_yield(void) {
  __asm__ volatile ("nop" ::: "memory");
}
#endif
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(__POWERPC__)
static inline void mi_atomic_yield(void) {
  __asm__ __volatile__ ("or 27,27,27" ::: "memory");
}
#endif
#elif defined(__sun)
// Fallback for other archs
#include <synch.h>
static inline void mi_atomic_yield(void) {
  smt_pause();
}
#else
#include <unistd.h>
static inline void mi_atomic_yield(void) {
  sleep(0);
}
#endif


// ----------------------------------------------------------------------
// Locks 
// These do not have to be recursive and should be light-weight 
// in-process only locks. Only used for reserving arena's and to 
// maintain the abandoned list.
// ----------------------------------------------------------------------
#define mi_lock(lock)    for(bool _go = (mi_lock_acquire(lock),true); _go; (mi_lock_release(lock), _go=false) )

void _mi_error_message(int err, const char* fmt, ...);

#define mi_lock_t  pthread_mutex_t

static inline bool mi_lock_try_acquire(mi_lock_t* lock) {
  return (pthread_mutex_trylock(lock) == 0);
}
static inline void mi_lock_acquire(mi_lock_t* lock) {
  const int err = pthread_mutex_lock(lock);
  if (err != 0) {
    _mi_error_message(err, "internal error: lock cannot be acquired\n");
  }
}
static inline void mi_lock_release(mi_lock_t* lock) {
  pthread_mutex_unlock(lock);
}
static inline void mi_lock_init(mi_lock_t* lock) {
  pthread_mutex_init(lock, NULL);
}
static inline void mi_lock_done(mi_lock_t* lock) {
  pthread_mutex_destroy(lock);
}

#endif // __MIMALLOC_ATOMIC_H
