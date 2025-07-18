/* -*- C++ -*- */

/*

  The Hoard Multiprocessor Memory Allocator
  www.hoard.org

  Author: Emery Berger, http://www.emeryberger.com
  Copyright (c) 1998-2020 Emery Berger

  See the LICENSE file at the top-level directory of this
  distribution and at http://github.com/emeryberger/Hoard.

*/

/*
 * @file   libhoard.cpp
 * @brief  This file replaces malloc etc. in your application.
 * @author Emery Berger <http://www.emeryberger.com>
 */

#include <cstddef>
#include <new>

#include "VERSION.h"

#define versionMessage "Using the Hoard memory allocator (http://www.hoard.org), version " HOARD_VERSION_STRING "\n"

// Disable size checks in ANSIwrapper.
#define HL_NO_MALLOC_SIZE_CHECKS 0

#include "heaplayers.h"

// The undef below ensures that any pthread_* calls get strong
// linkage.  Otherwise, our versions here won't replace them.  It is
// IMPERATIVE that this line appear before any files get included.

#undef __GXX_WEAK__ 

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN

// Maximize the degree of inlining.
#pragma inline_depth(255)

// Turn inlining hints into requirements.
#define inline __forceinline
#pragma warning(disable:4273)
#pragma warning(disable: 4098)  // Library conflict.
#pragma warning(disable: 4355)  // 'this' used in base member initializer list.
#pragma warning(disable: 4074)	// initializers put in compiler reserved area.
#pragma warning(disable: 6326)  // comparison between constants.

#endif

#if HOARD_NO_LOCK_OPT
// Disable lock optimization.
volatile bool anyThreadCreated = true;
#else
// The normal case. See heaplayers/spinlock.h.
volatile bool anyThreadCreated = false;
#endif

#include "hoardtlab.h"

//
// The base Hoard heap.
//


/// Maintain a single instance of the main Hoard heap.

Hoard::HoardHeapType * getMainHoardHeap() {
  // This function is C++ magic that ensures that the heap is
  // initialized before its first use. First, allocate a static buffer
  // to hold the heap.

  static double thBuf[sizeof(Hoard::HoardHeapType) / sizeof(double) + 1];

  // Now initialize the heap into that buffer.
  static auto * th = new (thBuf) Hoard::HoardHeapType;
  return th;
}

TheCustomHeapType * getCustomHeap();

enum { MAX_LOCAL_BUFFER_SIZE = 256 * 131072 };
static char initBuffer[MAX_LOCAL_BUFFER_SIZE];
static char * initBufferPtr = initBuffer;

extern bool isCustomHeapInitialized();

#include "wrappers/generic-memalign.cpp"

extern "C" {

#if defined(__GNUG__)
  void * xxmalloc (size_t sz)
#else
  void * __attribute__((flatten)) xxmalloc (size_t sz) __attribute__((alloc_size(1))) __attribute((malloc))
#endif
  {
    if (isCustomHeapInitialized()) {
      void * ptr = getCustomHeap()->malloc (sz);
      if (ptr == nullptr) {
	fprintf(stderr, "INTERNAL FAILURE.\n");
	abort();
      }
      return ptr;
    }
    // We still haven't initialized the heap. Satisfy this memory
    // request from the local buffer.
    void * ptr = initBufferPtr;
    initBufferPtr += sz;
    if (initBufferPtr > initBuffer + MAX_LOCAL_BUFFER_SIZE) {
      abort();
    }
    return ptr;
  }

#if defined(__GNUG__)
  void xxfree (void * ptr)
#else
  void xxfree (void * ptr)
#endif
  {
    getCustomHeap()->free (ptr);
  }

 
#if defined(__GNUG__)
  void * xxmemalign (size_t alignment, size_t sz) {
#else
  void * xxmemalign (size_t alignment, size_t sz) {
#endif
    return generic_xxmemalign(alignment, sz);
  }
    
  size_t xxmalloc_usable_size (void * ptr) {
    return getCustomHeap()->getSize (ptr);
  }

  void xxmalloc_lock() {
    // Undefined for Hoard.
  }

  void xxmalloc_unlock() {
    // Undefined for Hoard.
  }

} // namespace Hoard

#if defined(__linux__) && !defined(__MUSL__)
// include gnuwrapper here to aid inlining of xxmalloc + friends
#include "wrappers/gnuwrapper.cpp"
#endif
