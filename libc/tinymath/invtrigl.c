/*-*- mode:c;indent-tabs-mode:t;c-basic-offset:8;tab-width:8;coding:utf-8   -*-│
│vi: set et ft=c ts=8 tw=8 fenc=utf-8                                       :vi│
╚──────────────────────────────────────────────────────────────────────────────╝
│                                                                              │
│  Musl Libc                                                                   │
│  Copyright © 2005-2014 Rich Felker, et al.                                   │
│                                                                              │
│  Permission is hereby granted, free of charge, to any person obtaining       │
│  a copy of this software and associated documentation files (the             │
│  "Software"), to deal in the Software without restriction, including         │
│  without limitation the rights to use, copy, modify, merge, publish,         │
│  distribute, sublicense, and/or sell copies of the Software, and to          │
│  permit persons to whom the Software is furnished to do so, subject to       │
│  the following conditions:                                                   │
│                                                                              │
│  The above copyright notice and this permission notice shall be              │
│  included in all copies or substantial portions of the Software.             │
│                                                                              │
│  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,             │
│  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF          │
│  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.      │
│  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY        │
│  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,        │
│  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE           │
│  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                      │
│                                                                              │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/math.h"
#include "libc/runtime/runtime.h"
#include "libc/tinymath/feval.internal.h"
#include "libc/tinymath/invtrigl.internal.h"
#include "libc/tinymath/kernel.internal.h"
#include "libc/tinymath/ldshape.internal.h"

asm(".ident\t\"\\n\\n\
fdlibm (fdlibm license)\\n\
Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.\"");
asm(".ident\t\"\\n\\n\
Musl libc (MIT License)\\n\
Copyright 2005-2014 Rich Felker, et. al.\"");
asm(".include \"libc/disclaimer.inc\"");
/* clang-format off */

#if LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
static const long double
pS0 =  1.66666666666666666631e-01L,
pS1 = -4.16313987993683104320e-01L,
pS2 =  3.69068046323246813704e-01L,
pS3 = -1.36213932016738603108e-01L,
pS4 =  1.78324189708471965733e-02L,
pS5 = -2.19216428382605211588e-04L,
pS6 = -7.10526623669075243183e-06L,
qS1 = -2.94788392796209867269e+00L,
qS2 =  3.27309890266528636716e+00L,
qS3 = -1.68285799854822427013e+00L,
qS4 =  3.90699412641738801874e-01L,
qS5 = -3.14365703596053263322e-02L;

const long double pio2_hi = 1.57079632679489661926L;
const long double pio2_lo = -2.50827880633416601173e-20L;

/* used in asinl() and acosl() */
/* R(x^2) is a rational approximation of (asin(x)-x)/x^3 with Remez algorithm */
long double __invtrigl_R(long double z)
{
	long double p, q;
	p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*(pS5+z*pS6))))));
	q = 1.0+z*(qS1+z*(qS2+z*(qS3+z*(qS4+z*qS5))));
	return p/q;
}
#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384
static const long double
pS0 =  1.66666666666666666666666666666700314e-01L,
pS1 = -7.32816946414566252574527475428622708e-01L,
pS2 =  1.34215708714992334609030036562143589e+00L,
pS3 = -1.32483151677116409805070261790752040e+00L,
pS4 =  7.61206183613632558824485341162121989e-01L,
pS5 = -2.56165783329023486777386833928147375e-01L,
pS6 =  4.80718586374448793411019434585413855e-02L,
pS7 = -4.42523267167024279410230886239774718e-03L,
pS8 =  1.44551535183911458253205638280410064e-04L,
pS9 = -2.10558957916600254061591040482706179e-07L,
qS1 = -4.84690167848739751544716485245697428e+00L,
qS2 =  9.96619113536172610135016921140206980e+00L,
qS3 = -1.13177895428973036660836798461641458e+01L,
qS4 =  7.74004374389488266169304117714658761e+00L,
qS5 = -3.25871986053534084709023539900339905e+00L,
qS6 =  8.27830318881232209752469022352928864e-01L,
qS7 = -1.18768052702942805423330715206348004e-01L,
qS8 =  8.32600764660522313269101537926539470e-03L,
qS9 = -1.99407384882605586705979504567947007e-04L;

const long double pio2_hi = 1.57079632679489661923132169163975140L;
const long double pio2_lo = 4.33590506506189051239852201302167613e-35L;

long double __invtrigl_R(long double z)
{
	long double p, q;
	p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*(pS5+z*(pS6+z*(pS7+z*(pS8+z*pS9)))))))));
	q = 1.0+z*(qS1+z*(qS2+z*(qS3+z*(qS4+z*(qS5+z*(qS6+z*(qS7+z*(qS8+z*qS9))))))));
	return p/q;
}
#endif
