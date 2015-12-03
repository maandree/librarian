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
	const char *lower;

  	/**
	 * The highest acceptable version.
	 * `NULL` if unbounded.
	 */
	const char *upper;

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
}

