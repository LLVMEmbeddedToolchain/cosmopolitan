/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2021 Justine Alexandra Roberts Tunney                              │
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
#include "libc/dce.h"
#include "libc/intrin/asan.internal.h"
#include "libc/nexgen32e/x86feature.h"
#include "libc/str/str.h"

typedef char xmm_t __attribute__((__vector_size__(16), __aligned__(16)));

static inline const char *strchrnul_pure(const char *s, int c) {
  for (;; ++s) {
    if ((*s & 255) == (c & 255)) return s;
    if (!*s) return s;
  }
}

#ifdef __x86_64__
noasan static inline const char *strchrnul_sse(const char *s, unsigned char c) {
  unsigned k;
  unsigned m;
  xmm_t v, *p;
  xmm_t z = {0};
  xmm_t n = {c, c, c, c, c, c, c, c, c, c, c, c, c, c, c, c};
  k = (uintptr_t)s & 15;
  p = (const xmm_t *)((uintptr_t)s & -16);
  v = *p;
  m = __builtin_ia32_pmovmskb128((v == z) | (v == n));
  m >>= k;
  m <<= k;
  while (!m) {
    v = *++p;
    m = __builtin_ia32_pmovmskb128((v == z) | (v == n));
  }
  return (const char *)p + __builtin_ctzl(m);
}
#endif

/**
 * Returns pointer to first instance of character.
 *
 * If c is not found then a pointer to the nul byte is returned.
 *
 * @param s is a NUL-terminated string
 * @param c is masked with 255 as byte to search for
 * @return pointer to first instance of c, or pointer to
 *     NUL terminator if c is not found
 */
char *strchrnul(const char *s, int c) {
#ifdef __x86_64__
  const char *r;
  if (X86_HAVE(SSE)) {
    if (IsAsan()) __asan_verify(s, 1);
    r = strchrnul_sse(s, c);
  } else {
    r = strchrnul_pure(s, c);
  }
  _unassert((*r & 255) == (c & 255) || !*r);
  return (char *)r;
#else
  return strchrnul_pure(s, c);
#endif
}
