/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/assert.h"
#include "libc/calls/calls.h"
#include "libc/errno.h"
#include "libc/intrin/atomic.h"
#include "libc/intrin/pthread.h"

#define INIT     0
#define CALLING  1
#define FINISHED 2

/**
 * Ensures initialization function is called exactly once, e.g.
 *
 *     static void *g_factory;
 *
 *     static void InitFactory(void) {
 *       g_factory = expensive();
 *     }
 *
 *     void *GetFactory(void) {
 *       static pthread_once_t once = PTHREAD_ONCE_INIT;
 *       pthread_once(&once, InitFactory);
 *       return g_factory;
 *     }
 *
 * @return 0 on success, or errno on error
 */
int pthread_once(pthread_once_t *once, void init(void)) {
  char old;
  switch ((old = atomic_load_explicit(once, memory_order_relaxed))) {
    case INIT:
      if (atomic_compare_exchange_strong_explicit(once, &old, CALLING,
                                                  memory_order_acquire,
                                                  memory_order_relaxed)) {
        init();
        atomic_store(once, FINISHED);
        break;
      }
      // fallthrough
    case CALLING:
      do {
        pthread_yield();
      } while (atomic_load_explicit(once, memory_order_relaxed) == CALLING);
      break;
    case FINISHED:
      break;
    default:
      assert(!"bad once");
      return EINVAL;
  }
  return 0;
}
