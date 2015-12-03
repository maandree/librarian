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
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define  t(...)  do { if (__VA_ARGS__) goto fail; } while (0)


/**
 * The name of the process.
 */
static const char *argv0;


/**
 * A library and version range.
 */
struct library {
	/**
	 * The name of the library.
	 */
	const char *name;

  	/**
	 * The lowest acceptable version.
	 * `NULL` if unbounded.
	 */
	char *lower;

  	/**
	 * The highest acceptable version.
	 * `NULL` if unbounded.
	 */
	char *upper;

  	/**
	 * Is the version stored in
	 * `lower` acceptable.
	 */
	int lower_closed;

  	/**
	 * Is the version stored in
	 * `ypper` acceptable.
	 */
	int upper_closed;
};



/**
 * Determine whether a string is the
 * name of a non-reserved variable.
 * 
 * @param   s  The string.
 * @return     1: The string is a varible name.
 *             0: The string is a library.
 */
static int is_variable(const char *s)
{
	for (; *s; s++) {
		if      (isupper(*s))       ;
		else if (isdigit(*s))       ;
		else if (strchr("_-", *s))  ;
		else
			return 0;
	}
	return 1;
}


/**
 * Parse a library–library-version range
 * argument.
 * 
 * @param   s    The string.
 * @param   lib  Output parameter for the library spec:s.
 * @return       0: Successful.
 *               1: Syntax error.
 */
static int parse_library(char *s, struct library *lib)
{
	char *p;
	char c;

	memset(lib, 0, sizeof(*lib));

	if (strchr(s, '/') || strchr("<>=", *s))
		return 1;

	lib->name = s;
	p = strpbrk(s, "<>=");
	if (p == NULL)
		return 0;
	c = *p, *p++ = '\0';

	switch (c) {
	case '=':
		lib->lower_closed = lib->upper_closed = 1;
		lib->lower = lib->upper = p;
		break;
	case '>':
		p += lib->lower_closed = (*p == '=');
		lib->lower = p;
		s = strchr(p, '<');
		if (s == NULL)
			break;
		*s++ = '\0';
		if (!*(p = s))
			return 0;
		/* fall through */
	case '<':
		p += lib->upper_closed = (*p == '=');
		lib->upper = p;
		break;
	default:
		assert(0);
		abort();
		break;
	}

	return (strpbrk(p, "<>=") || !*p);
}


/**
 * Compare two version numbers that do not
 * compare any does, and does not have an
 * epoch.
 * 
 * @param   a  One of the version numbers.
 * @param   b  The other version number.
 * @return     <0: `a` < `b`.
 *             =0: `a` = `b`.
 *             >1: `a` > `b`.
 */
static int version_subcmp(char *a, char *b)
{
	char *ap;
	char *bp;
	char ac, bc;
	int r;

	while (*a || *b) {
		/* Compare digit part. */
		/* (We must support arbitrary lenght.) */
		ap = a + strspn(a, "0123456789");
		bp = b + strspn(b, "0123456789");
		while (*a == '0')  a++;
		while (*b == '0')  b++;
		ac = *ap, bc = *bp;
		*ap = '\0', *bp = '\0';
		if (ap - a < bp - b)  return -1;
		if (ap - a > bp - b)  return +1;
		r = strcmp(a, b);
		*ap = ac, *bp = bc;
		a = *a ? (ap + 1) : a;
		b = *b ? (bp + 1) : b;
		if (r)  return r;

		/* Compare letter (non-digit) part. */
		ap = a + strcspn(a, "0123456789");
		bp = b + strcspn(b, "0123456789");
		ac = *ap, bc = *bp;
		*ap = '\0', *bp = '\0';
		r = strcmp(a, b);
		*ap = ac, *bp = bc;
		a = *a ? (ap + 1) : a;
		b = *b ? (bp + 1) : b;
		if (r)  return r;
	}

	return 0;
}


/**
 * Compare two version numbers.
 * 
 * @param   a  One of the version numbers.
 * @param   b  The other version number.
 * @return     <0: `a` < `b`.
 *             =0: `a` = `b`.
 *             >0: `a` > `b`.
 */
static int version_cmp(char *a, char *b)
{
#define COMPARE                              \
	if (ap && bp) {                      \
		ac = *ap, bc = *bp;          \
		*ap = *bp = '\0';            \
		r = version_subcmp(a, b);    \
		*ap = ac, *bp = bc;          \
		a = ap + 1, b = bp + 1;      \
	} else if (ap) {                     \
		ac = *ap, *ap = '\0';        \
		r = version_subcmp(a, nil);  \
		*ap = ac, a = ap + 1;        \
	} else if (bp) {                     \
		bc = *bp, *bp = '\0';        \
		r = version_subcmp(nil, b);  \
		*bp = bc, b = bp + 1;        \
	}                                    \
	if (r)  return r

	char *ap;
	char *bp;
	char ac, bc;
	int r = 0;
	static char nil[1] = { '\0' };

	/* Compare epoch. */
	ap = strchr(a, ':');
	bp = strchr(b, ':');
	COMPARE;

	/* Compare non-epoch */
	for (;;) {
		ap = strchr(a, '.');
		bp = strchr(b, '.');
		if (!ap && !ap)
			return 0;
		COMPARE;
	}

#undef COMPARE
}


/**
 * @return  0: Program was successful.
 *          1: An error occurred.
 *          2: A library was not found.
 *          3: Usage error.
 */
int main(int argc, char *argv[])
{
#define CLEANUP  \
	free(libraries)

	int dashed = 0, f_deps = 0, f_locate = 0, f_oldest = 0;
	char *arg;
	char **args = argv;
	char **args_last = argv;
	char **variables = argv;
	char **variables_last = argv;
	struct library *libraries = NULL;
	struct library *libraries_last;

	/* Parse arguments. */
	argv0 = argv ? (argc--, *argv++) : "pp";
	while (argc) {
		if (!dashed && !strcmp(*argv, "--")) {
			dashed = 1;
			argv++;
			argc--;
		} else if (!dashed && (**argv == '-')) {
			arg = *argv++;
			argc--;
			if (!*arg)
				goto usage;
			for (arg++; *arg; arg++) {
				if (*arg == 'd')
					f_deps = 1;
				else if (*arg == 'l')
					f_locate = 1;
				else if (*arg == 'o')
					f_oldest = 1;
				else
					goto usage;
			}
		} else {
			*args_last++ = *argv++;
			argc--;
		}
	}

	/* Parse VARIABLE and LIBRARY arguments. */
	libraries = malloc((size_t)(args_last - args) * sizeof(*libraries));
	libraries_last = libraries;
	t (libraries == NULL);
	for (; args != args_last; args++) {
		if (is_variable(*args))
			*variables_last = *args;
		else if (parse_library(*args, libraries_last++))
			goto usage;
	}

	CLEANUP;
	return 0;

fail:
	perror(argv0);
	CLEANUP;
	return 1;

usage:
	fprintf(stderr, "%s: Invalid arguments, see `man 1 librarian'.\n", argv0);
	CLEANUP;
	return 3;

#undef CLEANUP
}

