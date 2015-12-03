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
#include "util.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>


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
 * Structure for already located librarian files.
 */
struct found_file {
	/**
	 * The name of the library.
	 */
	const char *name;

	/**
	 * The found version of the library.
	 */
	char *version;

	/**
	 * The path name of the librarian file.
	 */
	char *path;
};



/**
 * The name of the process.
 */
static const char *argv0;

/**
 * Sorted list of already located librarian files.
 */
static struct found_file *found_files = NULL;

/**
 * The number of elements in `found_files`.
 */
static size_t found_files_count = 0;



/**
 * Compares the name of two libraries.
 * 
 * @param   a:const struct library *  One of the libraries.
 * @param   b:const struct library *  The other library.
 * @return                            <0: `a` < `b`.
 *                                    =0: `a` = `b`.
 *                                    >0: `a` > `b`.
 */
static int library_name_cmp(const void *a, const void *b)
{
	const struct library *la = a;
	const struct library *lb = b;
	return strcmp(la->name, lb->name);
}


/**
 * Compares the name of two `struct found_file`.
 * 
 * @param   a:const struct found_file *  One of the files.
 * @param   b:const struct found_file *  The other file.
 * @return                               <0: `a` < `b`.
 *                                       =0: `a` = `b`.
 *                                       >0: `a` > `b`.
 */
static int found_file_name_cmp(const void *a, const void *b)
{
	const struct found_file *fa = a;
	const struct found_file *fb = b;
	return strcmp(fa->name, fb->name);
}


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
	for (; *s; s++)
		if (!isupper(*s) && !isdigit(*s) && !strchr("_-", *s))
			return 0;
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
		NEVER_REACHED;
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
	int r;

	while (*a || *b) {
		/* Compare digit part. */
		/* (We must support arbitrary lenght.) */
		ap = a + strspn(a, "0123456789");
		bp = b + strspn(b, "0123456789");
		while (*a == '0')  a++;
		while (*b == '0')  b++;
		if (ap - a < bp - b)  return -1;
		if (ap - a > bp - b)  return +1;
		TEMP_NUL(ap, TEMP_NUL(bp, r = strcmp(a, b)));
		a = isdigit(*a) ? (ap + 1) : a;
		b = isdigit(*b) ? (bp + 1) : b;
		if (r)  return r;

		/* Compare letter (non-digit) part. */
		ap = a + strcspn(a, "0123456789");
		bp = b + strcspn(b, "0123456789");
		TEMP_NUL(ap, TEMP_NUL(bp, r = strcmp(a, b)));
		a = *a && !isdigit(*a) ? (ap + 1) : a;
		b = *b && !isdigit(*b) ? (bp + 1) : b;
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
#define COMPARE  \
	if (ap && bp) {  \
		TEMP_NUL(ap, TEMP_NUL(bp, r = version_subcmp(a, b)));  \
	} else if (ap) {  \
		TEMP_NUL(ap, r = version_subcmp(a, nil));  \
	} else if (bp) {  \
		TEMP_NUL(bp, r = version_subcmp(nil, b));  \
	}  \
	a = ap ? (ap + 1) : a;  \
	b = bp ? (bp + 1) : b;  \
	if (r)  return r
#define END_AT(C)  ap = strchr(a, (C)), bp = strchr(b, (C))

	char *ap;
	char *bp;
	int r = 0;
	static char nil[1] = { '\0' };

	/* Compare epoch. */
	END_AT(':');
	COMPARE;

	/* Compare non-epoch */
	while (END_AT('.'), (ap || bp)) {
		COMPARE;
	}
	END_AT('\0');
	COMPARE;
	return 0;
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

	upper = required->upper_closed ? (upper <= 0) : (upper < 0);
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
	void *new;
	char *best = NULL;
	char *best_ver;
	int r;

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
		if (best != NULL) {
			GET_VERSION(best_ver, best);
			r = version_cmp(p, best_ver + 1);
			if (!(oldest ? (r < 0) : (r > 0)))
				continue;
		}
		new = strdup(f->d_name);
		t (new == NULL);
		free(best), best = new;
	}
	t (errno);

	closedir(d), d = NULL;

	if (best == NULL)
		return errno = 0, NULL;

	p = malloc(strlen(path) + strlen(best) + 2);
	t (p == NULL);
	stpcpy(stpcpy(stpcpy(p, path), "/"), best);

	free(best);
	return p;

fail:
	RETURN (NULL) {
	free(best);
	if (d != NULL)
		closedir(d);
	}
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
	int r;

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
			GET_VERSION(best_ver, best);
			GET_VERSION(found_ver, found);
			r = version_cmp(found_ver + 1, best_ver + 1);
			if (oldest ? (r < 0) : (r > 0))
				old = best, best = found;
		}
		free(old);
	}

	return errno = 0, best;

fail:
	RETURN (NULL)
	free(best);
}


/**
 * Find librarian files for all libraries.
 * 
 * Found files are appended to `found_files`.
 * 
 * @param   libraries  The sought libraries.
 * @param   n          The number of elements in `libraries`.
 * @param   path       LIBRARIAN_PATH.
 * @param   oldest     Are older versions prefered?
 * @return             0:             Successful and found all files.
 *                     -1 and !errno: Did not find all files, but otherwise successful.
 *                     -1 and errno:  An error occurred
 */
static int find_librarian_files(struct library *libraries, size_t n, char *path, int oldest)
{
	size_t i;
	char *best = NULL;
	char *found;
	char *best_ver;
	char *found_ver;
	const char *last = NULL;
	size_t ffc = found_files_count;
	int r;
	struct found_file f;
	struct found_file *have;

	qsort(libraries, n, sizeof(*libraries), library_name_cmp);
	qsort(found_files, ffc, sizeof(*found_files), found_file_name_cmp);
	REALLOC(found_files, ffc + n);

	for (i = 0; i < n; i++) {
		f.name = libraries[i].name;
		have = bsearch(&f, found_files, ffc, sizeof(*found_files), found_file_name_cmp);
		if (have) {
			if (test_library_version(have->version, libraries + i))
				continue;
			goto not_this_range;
		}
		found = locate(libraries + i, path, oldest);
		t (!found && errno);
		if (found == NULL)
			goto not_this_range;
		if (last && !strcmp(f.name, last)) {
			GET_VERSION(best_ver, best);
			GET_VERSION(found_ver, found);
			r = version_cmp(found + 1, best_ver + 1);
			if (!(oldest ? (r < 0) : (r > 0)))
				continue;
			free(best);
		} else {
			last = f.name, found_files_count++;
		}
		GET_VERSION(found_ver, found);
		found_files[found_files_count - 1].name = f.name;
		found_files[found_files_count - 1].version = found_ver + 1;
		found_files[found_files_count - 1].path = best = found;
		continue;

	not_this_range:
		if ((i + 1 == n) || strcmp(f.name, libraries[i + 1].name))
			goto not_found;
		continue;
	}

	return 0;

not_found:
	if (libraries[i].upper == libraries[i].lower) {
		fprintf(stderr, "%s: cannot find library: %s%s%s", argv0,
			libraries[i].name, libraries[i].upper ? "=" : "",
			libraries[i].upper ? libraries[i].upper : "");
	} else {
		fprintf(stderr, "%s: cannot find library: %s%s%s%s%s%s%s", argv0,
			libraries[i].name,
			libraries[i].lower ? ">" : "", libraries[i].lower_closed ? "=" : "",
			libraries[i].lower ? libraries[i].lower : "",
			libraries[i].upper ? "<" : "", libraries[i].upper_closed ? "=" : "",
			libraries[i].upper ? libraries[i].upper : "");
	}
	errno = 0;
fail:
	return -1;
}


/**
 * Read the value of a variable in a file.
 * 
 * @param   path  The pathname of the file to read.
 * @param   var   The variable to retrieve
 * @return        The value of variable. `NULL` on error or if
 *                not found, `errno` is set to 0 if not found.
 */
static char *find_variable(const char *path, const char *var)
{
	int fd = -1;
	size_t ptr = 0, size = 0, len;
	char *buffer = NULL;
	char *p;
	char *q;
	char *sought = NULL;
	ssize_t n;

	fd = open(path, O_RDONLY);
	t (fd == -1);

	for (;;) {
		MAYBE_GROW(buffer, ptr, size, 512);
		n = read(fd, buffer + ptr, size - ptr);
		t (n < 0);
		if (n == 0)
			break;
		ptr += (size_t)n;
	}

	close(fd), fd = -1;
	REALLOC(buffer, ptr + 3);
	buffer[ptr++] = '\n';
	buffer[ptr++] = '\0';
	memmove(buffer + 1, buffer, ptr);
	*buffer = '\n';

	sought = malloc(strlen(var) + 2);
	t (sought == NULL);
	sought[0] = '\n';
	strcpy(sought + 1, var);

	len = strlen(sought);
	for (p = buffer; p;) {
		p = strstr(p, sought);
		if (p == NULL)
			break;
		if (!isspace(p[len])) {
			p = strchr(p + 1, '\n');
			continue;
		}
		p += len + 1;
		q = strchr(p, '\n');
		*q = '\0';
		break;
	}

	if (p != NULL) {
		p = strdup(p);
		t (p == NULL);
	}
	free(sought);
	free(buffer);
	return p;

fail:
	RETURN (NULL) {
	if (fd >= 0)
		close(fd);
	free(buffer);
	free(sought);
	}
}


/**
 * Get variables values stored in librarian files.
 * 
 * @param   vars         Pointer to the first variable.
 * @param   vars_end     Pointer to just after the last variable.
 * @param   files_start  The index of the first file in `found_files`
 *                       for which variables should be retrieved.
 * @return               String with all variables, `NULL` on error.
 */
static char *get_variables(const char **vars, const char **vars_end, size_t files_start)
{
	char *path;
	const char **var;
	char **parts = NULL;
	char *part;
	size_t ptr = 0;
	size_t size = 0;
	size_t len = 0;
	char *rc;
	char *p;

	while (files_start < found_files_count) {
		path = found_files[files_start++].path;
		for (var = vars; var != vars_end; var++) {
			part = find_variable(path, *var);
			t (!part && errno);
			if (!part || !*part)
				continue;
			MAYBE_GROW(parts, ptr, size, 8);
			len += strlen(part) + 1;
			parts[ptr++] = part;
		}
	}

	if (len == 0)
		return strdup("");

	p = rc = malloc(len);
	t (rc == NULL);
	for (size = ptr, ptr = 0; ptr < size; ptr++) {
		p = stpcpy(p, parts[ptr]);
		*p++ = ' ';
		free(parts[ptr]);
	}
	free(parts);
	p[-1] = 0;

	return rc;
fail:
	RETURN (NULL) {
	while (ptr--)
		free(parts[ptr]);
	free(parts);
	}
}


/**
 * @return  0: Program was successful.
 *          1: An error occurred.
 *          2: A library was not found.
 *          3: Usage error.
 */
int main(int argc, char *argv[])
{
	int dashed = 0, f_deps = 0, f_locate = 0, f_oldest = 0;
	char *arg;
	char **args = argv;
	char **args_last = args;
	const char **variables = (const char **)argv;
	const char **variables_last = variables;
	struct library *libraries = NULL;
	size_t libraries_ptr = 0;
	size_t libraries_size = 0;
	const char *path_;
	char *path = NULL;
	int rc;
	size_t start_files;
	size_t start_libs, n;
	char *data = NULL;
	char *s;
	char *end;
	void** free_this = NULL;
	size_t free_this_ptr = 0;
	size_t free_this_size = 0;
	const char *deps_string = "deps";

	/* Parse arguments. */
	argv0 = argv ? (argc--, *argv++) : "pp";
	while (argc--) {
		if (!dashed && !strcmp(*argv, "--")) {
			dashed = 1;
			argv++;
		} else if (!dashed && (**argv == '-')) {
			arg = *argv++;
			if (!*arg)
				goto usage;
			for (arg++; *arg; arg++) {
				if      (*arg == 'd')  f_deps = 1;
				else if (*arg == 'l')  f_locate = 1;
				else if (*arg == 'o')  f_oldest = 1;
				else                   goto usage;
			}
		} else {
			*args_last++ = *argv++;
		}
	}
	if (f_deps && f_locate)
		goto usage;

	/* Parse VARIABLE and LIBRARY arguments. */
	libraries_size = (size_t)(args_last - args);
	libraries = malloc(libraries_size * sizeof(*libraries));
	t (libraries == NULL);
	for (; args != args_last; args++) {
		if (is_variable(*args))
			*variables_last++ = *args;
		else if (parse_library(*args, libraries + libraries_ptr++))
			goto usage;
	}

	/* Get LIBRARIAN_PATH. */
	path_ = getenv("LIBRARIAN_PATH");
	if (!path_ || !*path_)
		path_ = DEFAULT_PATH;
	path = strdup(path_);
	t (path == NULL);

	/* Find librarian files. */
	for (start_libs = 0; (n = libraries_ptr - start_libs);) {
		start_files = found_files_count;
		if (find_librarian_files(libraries + start_libs, n, path, f_oldest)) {
			t (errno);
			goto not_found;
		}
		start_libs += n;
		if (f_locate || !f_deps)
			break;
		data = get_variables(&deps_string, 1 + &deps_string, start_files);
		t (data == NULL);
		for (end = s = data; end; s = end + 1) {
			while (isspace(*s))
				s++;
			if ((end = strpbrk(s, " \t\r\n\f\v")))
				*end = '\0';
			MAYBE_GROW(libraries, libraries_ptr, libraries_size, 1);
			if (*s && parse_library(s, libraries + libraries_ptr++))
				goto not_found;
		}
		MAYBE_GROW(free_this, free_this_ptr, free_this_size, 4);
		free_this[free_this_ptr++] = data, data = NULL;
	}
	if (f_locate) {
		for (n = 0; n < found_files_count; n++)
			t (printf("%s\n", found_files[n].path) < 0);
		goto done;
	}

	/* Print requested data. */
	data = get_variables(variables, variables_last, 0);
	t (data == NULL);
	t (printf("%s\n", data) < 0);

done:
	rc = 0;
	goto cleanup;
fail:
	perror(argv0);
	rc = 1;
	goto cleanup;
not_found:
	rc = 2;
	goto cleanup;
usage:
	fprintf(stderr, "%s: Invalid arguments, see `man 1 librarian'.\n", argv0);
	rc = 3;
	goto cleanup;

cleanup:
	while (found_files_count--)
		free(found_files[found_files_count].path);
	free(found_files);
	while (free_this_ptr--)
		free(free_this[free_this_ptr]);
	free(free_this);
	free(libraries);
	free(path);
	free(data);
	return rc;
}

