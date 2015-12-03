/**
 * MIT/X Consortium License
 * 
 * Copyright © 2015  Mattias Andrée <maandree@member.fsf.org>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>


#define  t(...)  do { if (__VA_ARGS__) goto fail; } while (0)


#define REALLOC(PTR, SIZE)  \
	do {  \
		void *new__;  \
		new__ = realloc(PTR, (SIZE) * sizeof(*(PTR)));  \
		t (new__ == NULL);  \
		(PTR) = new__;  \
	} while (0)


#define GROW(PTR, SIZE, DEFAULT)  \
	do {  \
		(SIZE) = (SIZE) ? ((SIZE) << 1) : (DEFAULT);  \
		REALLOC(PTR, SIZE);  \
	} while (0)


#define MAYBE_GROW(PTR, USED, SIZE, DEFAULT)  \
	do {  \
		if ((USED) == (SIZE))  \
			GROW(PTR, SIZE, DEFAULT);  \
	} while (0)


#define GET_VERSION(VER, PATH)  \
	do {  \
		(VER) = strrchr((PATH), '=');  \
		assert((VER) && !strchr((VER), '/'));  \
	} while (0)


#define NEVER_REACHED  \
	do {  \
		assert(0);  \
		abort();  \
	} while (0)


#define RETURN(R)  \
	for (int errno__ = errno, x__ = 0;; errno = errno__, x__++)  \
		if (x__) return (R); else


#define TEMP_NUL(P, ACTION)  \
	do {  \
		char P##__c  = *(P);  \
		*(P) = '\0';  \
		ACTION;  \
		*(P) = P##__c;  \
	} while (0)

