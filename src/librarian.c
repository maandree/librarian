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
#define _POSIX_C_SOURCE  200809L
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>

#define  t(...)  do { if (__VA_ARGS__) goto fail; } while (0)



/**
 * Default value for the environment variable LIBRARIAN_PATH.
 */
#ifndef DEFAULT_PATH
# define DEFAULT_PATH  "/usr/local/share/librarian:/usr/share/librarian"
#endif



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
 * The name of the process.
 */
static const char *argv0;



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
 * Test whether a version of a library is compatible.
 * 
 * @param   version   The found version.
 * @param   required  Compatible version range.
 * @return            1: Version is accepted.
 *                    0: Version is incompatible.
 */
static int test_library_version(char *version, struct library *required)
{
	int upper = required->upper ? version_cmp(version, required->upper) : -1;
	int lower = required->lower ? version_cmp(version, required->lower) : +1;

	upper = required->lower_closed ? (upper <= 0) : (upper < 0);
	lower = required->lower_closed ? (lower >= 0) : (lower > 0);

	return upper && lower;
}


/**
 * Locate a librarian file in a directory.
 * 
 * @param   lib     Library specification.
 * @param   path    The pathname of the directory.
 * @param   oldest  Are older versions prefered?
 * @return          The pathname of the library's librarian file.
 *                  `NULL` on error or if not found, if not found,
 *                  `errno` is set to 0.
 */
static char *locate_in_dir(struct library *lib, char *path, int oldest)
{
	DIR *d = NULL;
	struct dirent *f;
	char *p;
	char *old;
	char *best = NULL;
	char *best_ver;
	int r, saved_errno;

	d = opendir(path);
	t (d == NULL);

	while ((f = (errno = 0, readdir(d)))) {
		p = strrchr(f->d_name, '=');
		if (p == NULL)
			continue;
		*p = '\0';
		if (strcmp(f->d_name, lib->name))
			continue;
		*p++ = '=';
		if (!test_library_version(p, lib))
			continue;
		if (best == NULL) {
			old = best, best = strdup(f->d_name);
		} else {
			best_ver = strrchr(best, '=');
			assert(best_ver && !strchr(best_ver, '/'));
			r = version_cmp(p, best_ver + 1);
			if (oldest ? (r < 0) : (r > 0))
				old = best, best = strdup(f->d_name);
		}
		if (best == NULL) {
			best = old;
			goto fail;
		}
		free(old);
	}
	t (errno);

	closedir(d), d = NULL;

	if (best == NULL)
		return errno = 0, NULL;

	p = malloc(strlen(path) + strlen(best) + 2);
	t (p == NULL);
	stpcpy(stpcpy(stpcpy(p, path), "/"), best);

	return p;

fail:
	saved_errno = errno;
	free(best);
	if (d != NULL)
		closedir(d);
	errno = saved_errno;
	return NULL;
}


/**
 * Locate a librarian file on the system.
 * 
 * @param   lib     Library specification.
 * @param   path    LIBRARIAN_PATH.
 * @param   oldest  Are older versions prefered?
 * @return          The pathname of the library's librarian file.
 *                  `NULL` on error or if not found, if not found,
 *                  `errno` is set to 0.
 */
static char *locate(struct library *lib, char *path, int oldest)
{
	char *p;
	char *end = path;
	char *e;
	char *best = NULL;
	char *found;
	char *old;
	char *best_ver;
	char *found_ver;
	int r, saved_errno;

	for (p = path; end; *e = (end ? ':' : '\0'), p = end + 1) {
		end = strchr(p, ':');
		e = end ? end : strchr(p, '\0');
		*e = '\0';
		if (!*p)
			continue;
		found = locate_in_dir(lib, p, oldest);
		if (found == NULL) {
			t (errno);
			continue;
		}
		old = found;
		if (best == NULL) {
			old = best, best = found;
		} else {
			best_ver = strrchr(best, '=');
			found_ver = strrchr(found, '=');
			assert(best_ver && !strchr(best_ver, '/'));
			assert(found_ver && !strchr(found_ver, '/'));
			r = version_cmp(found_ver + 1, best_ver + 1);
			if (oldest ? (r < 0) : (r > 0))
				old = best, best = found;
		}
		free(old);
	}

	return errno = 0, best;

fail:
	saved_errno = errno;
	free(best);
	errno = saved_errno;
	return NULL;
}


/**
 * @return  0: Program was successful.
 *          1: An error occurred.
 *          2: A library was not found.
 *          3: Usage error.
 */
int main(int argc, char *argv[])
{
#define CLEANUP           \
	free(libraries),  \
	free(path)

	int dashed = 0, f_deps = 0, f_locate = 0, f_oldest = 0;
	char *arg;
	char **args = argv;
	char **args_last = argv;
	char **variables = argv;
	char **variables_last = argv;
	struct library *libraries = NULL;
	struct library *libraries_last;
	const char *path_;
	char *path = NULL;

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
	if (f_deps && f_locate)
		goto usage;

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

	/* Get LIBRARIAN_PATH. */
	path_ = getenv("LIBRARIAN_PATH");
	if (!path_ || !*path_)
		path_ = DEFAULT_PATH;
	path = strdup(path_);
	t (path == NULL);

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

