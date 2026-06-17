/*
 * qglib_util.c — glib string/format/memory/filesystem/time primitives.
 * See qglib_util.h. POSIX-targeted (Android/Linux). The differential test
 * (test_util.c) pins behaviour against real glib.
 */
#define _POSIX_C_SOURCE 200809L /* expose clock_gettime, lstat, mkdir under -std=c11 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "qglib_util.h"

/* ---- memory (glib: malloc(0)/realloc(.,0) yield NULL) ---- */

gpointer g_malloc(gsize n_bytes) {
	return n_bytes ? QG_MALLOC(n_bytes) : NULL;
}

gpointer g_malloc0(gsize n_bytes) {
	return n_bytes ? QG_CALLOC(1, n_bytes) : NULL;
}

gpointer g_realloc(gpointer mem, gsize n_bytes) {
	if (n_bytes == 0) {
		QG_FREE(mem);
		return NULL;
	}
	return QG_REALLOC(mem, n_bytes);
}

void g_free(gpointer mem) {
	QG_FREE(mem);
}

void g_error_free(GError *error) {
	if (error) {
		g_free(error->message);
		g_free(error);
	}
}

/* ---- strings (BSD strlcpy/strlcat semantics, as in glib) ---- */

gsize g_strlcpy(gchar *dest, const gchar *src, gsize dest_size) {
	gchar *d = dest;
	const gchar *s = src;
	gsize n = dest_size;
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0') {
				break;
			}
		}
	}
	if (n == 0) {
		if (dest_size != 0) {
			*d = '\0';
		}
		while (*s++) {
			/* count the rest of src */
		}
	}
	return (gsize) (s - src - 1);
}

gsize g_strlcat(gchar *dest, const gchar *src, gsize dest_size) {
	gchar *d = dest;
	const gchar *s = src;
	gsize n = dest_size;
	while (n-- != 0 && *d != '\0') {
		d++;
	}
	const gsize dlen = (gsize) (d - dest);
	n = dest_size - dlen;
	if (n == 0) {
		return dlen + strlen(s);
	}
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';
	return dlen + (gsize) (s - src);
}

gint g_snprintf(gchar *string, gulong n, const gchar *format, ...) {
	va_list ap;
	va_start(ap, format);
	const gint r = vsnprintf(string, n, format, ap);
	va_end(ap);
	return r;
}

static gint qg_ascii_lower(gint c) {
	return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

gint g_ascii_strcasecmp(const gchar *s1, const gchar *s2) {
	for (;;) {
		const gint c1 = qg_ascii_lower((guchar) *s1);
		const gint c2 = qg_ascii_lower((guchar) *s2);
		if (c1 != c2) {
			return c1 - c2;
		}
		if (c1 == 0) {
			return 0;
		}
		s1++;
		s2++;
	}
}

gint g_ascii_strncasecmp(const gchar *s1, const gchar *s2, gsize n) {
	while (n-- > 0) {
		const gint c1 = qg_ascii_lower((guchar) *s1);
		const gint c2 = qg_ascii_lower((guchar) *s2);
		if (c1 != c2) {
			return c1 - c2;
		}
		if (c1 == 0) {
			return 0;
		}
		s1++;
		s2++;
	}
	return 0;
}

gint64 g_ascii_strtoll(const gchar *nptr, gchar **endptr, guint base) {
	return (gint64) strtoll(nptr, endptr, (int) base);
}

gboolean g_ascii_isalnum(gchar c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

gboolean g_str_has_prefix(const gchar *str, const gchar *prefix) {
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

gint g_strcmp0(const gchar *str1, const gchar *str2) {
	if (!str1) {
		return -(str1 != str2);
	}
	if (!str2) {
		return str1 != str2;
	}
	return strcmp(str1, str2);
}

gboolean g_str_has_suffix(const gchar *str, const gchar *suffix) {
	const gsize sl = strlen(str);
	const gsize fl = strlen(suffix);
	return fl <= sl && strcmp(str + sl - fl, suffix) == 0;
}

gchar *g_strdup(const gchar *str) {
	if (!str) {
		return NULL;
	}
	const gsize n = strlen(str) + 1;
	gchar *p = QG_MALLOC(n);
	memcpy(p, str, n);
	return p;
}

gchar *g_strdup_printf(const gchar *format, ...) {
	va_list ap, ap2;
	va_start(ap, format);
	va_copy(ap2, ap);
	const gint len = vsnprintf(NULL, 0, format, ap);
	va_end(ap);
	gchar *p = QG_MALLOC((gsize) len + 1);
	vsnprintf(p, (gsize) len + 1, format, ap2);
	va_end(ap2);
	return p;
}

gchar **g_strsplit(const gchar *string, const gchar *delimiter, gint max_tokens) {
	const gsize dl = strlen(delimiter);
	gsize cap = 8, n = 0;
	gchar **vec = QG_MALLOC(cap * sizeof(gchar *));
	if (*string != '\0') {
		const gchar *p = string;
		for (;;) {
			const gchar *next = NULL;
			if (!(max_tokens > 0 && (gint) n == max_tokens - 1)) {
				next = strstr(p, delimiter);
			}
			const gsize toklen = next ? (gsize) (next - p) : strlen(p);
			gchar *tok = QG_MALLOC(toklen + 1);
			memcpy(tok, p, toklen);
			tok[toklen] = '\0';
			if (n + 2 > cap) {
				cap *= 2;
				vec = QG_REALLOC(vec, cap * sizeof(gchar *));
			}
			vec[n++] = tok;
			if (!next) {
				break;
			}
			p = next + dl;
		}
	}
	vec[n] = NULL;
	return vec;
}

void g_strfreev(gchar **str_array) {
	if (str_array) {
		for (gsize i = 0; str_array[i] != NULL; i++) {
			QG_FREE(str_array[i]);
		}
		QG_FREE(str_array);
	}
}

guint g_strv_length(gchar **str_array) {
	guint n = 0;
	if (str_array) {
		while (str_array[n] != NULL) {
			n++;
		}
	}
	return n;
}

static int qg_ascii_isspace(int c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

gchar *g_strchug(gchar *string) {
	guchar *s = (guchar *) string;
	while (*s != '\0' && qg_ascii_isspace(*s)) {
		s++;
	}
	memmove(string, s, strlen((gchar *) s) + 1);
	return string;
}

gchar *g_strchomp(gchar *string) {
	gsize len = strlen(string);
	while (len > 0 && qg_ascii_isspace((guchar) string[len - 1])) {
		string[--len] = '\0';
	}
	return string;
}

gchar *g_strrstr(const gchar *haystack, const gchar *needle) {
	const gsize hl = strlen(haystack);
	const gsize nl = strlen(needle);
	if (nl == 0) {
		return (gchar *) (haystack + hl);
	}
	if (nl > hl) {
		return NULL;
	}
	for (gsize i = hl - nl + 1; i-- > 0;) {
		if (memcmp(haystack + i, needle, nl) == 0) {
			return (gchar *) (haystack + i);
		}
	}
	return NULL;
}

gchar *g_ascii_strdown(const gchar *str, gssize len) {
	if (len < 0) {
		len = (gssize) strlen(str);
	}
	gchar *r = QG_MALLOC((gsize) len + 1);
	for (gssize i = 0; i < len; i++) {
		const gchar c = str[i];
		r[i] = (c >= 'A' && c <= 'Z') ? (gchar) (c - 'A' + 'a') : c;
	}
	r[len] = '\0';
	return r;
}

gchar *g_path_get_basename(const gchar *file_name) {
	if (*file_name == '\0') {
		return g_strdup(".");
	}
	gsize len = strlen(file_name);
	while (len > 1 && file_name[len - 1] == G_DIR_SEPARATOR) {
		len--;
	}
	gsize base = len;
	while (base > 0 && file_name[base - 1] != G_DIR_SEPARATOR) {
		base--;
	}
	const gsize n = len - base;
	if (n == 0) {
		return g_strdup(G_DIR_SEPARATOR_S);
	}
	gchar *r = QG_MALLOC(n + 1);
	memcpy(r, file_name + base, n);
	r[n] = '\0';
	return r;
}

gchar *g_path_get_dirname(const gchar *file_name) {
	const gchar *base = strrchr(file_name, G_DIR_SEPARATOR);
	if (!base) {
		return g_strdup(".");
	}
	while (base > file_name && *base == G_DIR_SEPARATOR) {
		base--;
	}
	const gsize len = (gsize) (base - file_name) + 1;
	gchar *dir = QG_MALLOC(len + 1);
	memcpy(dir, file_name, len);
	dir[len] = '\0';
	return dir;
}

const gchar *g_get_home_dir(void) {
	static gchar *home = NULL;
	if (!home) {
		const char *e = getenv("HOME");
		home = g_strdup(e ? e : "");
	}
	return home;
}

const gchar *g_get_user_data_dir(void) {
	static gchar *dir = NULL;
	if (!dir) {
		const char *e = getenv("XDG_DATA_HOME");
		if (e && *e) {
			dir = g_strdup(e);
		} else {
			dir = g_strdup_printf("%s" G_DIR_SEPARATOR_S ".local" G_DIR_SEPARATOR_S "share", g_get_home_dir());
		}
	}
	return dir;
}

/* ---- filesystem + time ---- */

gchar *g_build_filename(const gchar *first_element, ...) {
	va_list ap;
	gsize cap = 64, len = 0;
	gchar *buf = QG_MALLOC(cap);
	buf[0] = '\0';
	va_start(ap, first_element);
	for (const gchar *el = first_element; el != NULL; el = va_arg(ap, const gchar *)) {
		const gchar *s = el;
		if (len > 0) {
			while (len > 0 && buf[len - 1] == G_DIR_SEPARATOR) {
				len--;
			}
			while (*s == G_DIR_SEPARATOR) {
				s++;
			}
			if (len + 2 > cap) {
				cap = (len + 2) * 2;
				buf = QG_REALLOC(buf, cap);
			}
			buf[len++] = G_DIR_SEPARATOR;
		}
		const gsize sl = strlen(s);
		if (len + sl + 1 > cap) {
			cap = (len + sl + 1) * 2;
			buf = QG_REALLOC(buf, cap);
		}
		memcpy(buf + len, s, sl);
		len += sl;
		buf[len] = '\0';
	}
	va_end(ap);
	return buf;
}

gchar *g_build_path(const gchar *separator, const gchar *first_element, ...) {
	const gsize seplen = strlen(separator);
	va_list ap;
	gsize cap = 64, len = 0;
	gchar *buf = QG_MALLOC(cap);
	buf[0] = '\0';
	va_start(ap, first_element);
	for (const gchar *el = first_element; el != NULL; el = va_arg(ap, const gchar *)) {
		const gchar *s = el;
		if (len > 0 && seplen > 0) {
			while (len >= seplen && memcmp(buf + len - seplen, separator, seplen) == 0) {
				len -= seplen;
			}
			while (strncmp(s, separator, seplen) == 0) {
				s += seplen;
			}
			if (len + seplen + 1 > cap) {
				cap = (len + seplen + 1) * 2;
				buf = QG_REALLOC(buf, cap);
			}
			memcpy(buf + len, separator, seplen);
			len += seplen;
		}
		const gsize sl = strlen(s);
		if (len + sl + 1 > cap) {
			cap = (len + sl + 1) * 2;
			buf = QG_REALLOC(buf, cap);
		}
		memcpy(buf + len, s, sl);
		len += sl;
		buf[len] = '\0';
	}
	va_end(ap);
	return buf;
}

gboolean g_file_test(const gchar *filename, GFileTest test) {
	if (test & G_FILE_TEST_IS_SYMLINK) {
		struct stat ls;
		if (lstat(filename, &ls) == 0 && S_ISLNK(ls.st_mode)) {
			return TRUE;
		}
	}
	struct stat st;
	if (stat(filename, &st) != 0) {
		return FALSE;
	}
	if (test & G_FILE_TEST_EXISTS) {
		return TRUE;
	}
	if ((test & G_FILE_TEST_IS_REGULAR) && S_ISREG(st.st_mode)) {
		return TRUE;
	}
	if ((test & G_FILE_TEST_IS_DIR) && S_ISDIR(st.st_mode)) {
		return TRUE;
	}
	if ((test & G_FILE_TEST_IS_EXECUTABLE) && (st.st_mode & S_IXUSR)) {
		return TRUE;
	}
	return FALSE;
}

gint g_mkdir_with_parents(const gchar *pathname, gint mode) {
	if (!pathname || !*pathname) {
		errno = EINVAL;
		return -1;
	}
	gchar *path = g_strdup(pathname);
	gint result = 0;
	for (gchar *p = path + (path[0] == G_DIR_SEPARATOR ? 1 : 0);; p++) {
		if (*p == G_DIR_SEPARATOR || *p == '\0') {
			const gchar c = *p;
			*p = '\0';
			if (path[0] != '\0' && mkdir(path, (mode_t) mode) != 0 && errno != EEXIST) {
				result = -1;
				*p = c;
				break;
			}
			*p = c;
			if (c == '\0') {
				break;
			}
		}
	}
	g_free(path);
	return result;
}

/* <glib/gstdio.h> shims (Quetoo uses g_fopen/g_remove/g_unlink) */
FILE *g_fopen(const gchar *filename, const gchar *mode) {
	return fopen(filename, mode);
}

gint g_remove(const gchar *filename) {
	return remove(filename);
}

gint g_unlink(const gchar *filename) {
	return unlink(filename);
}

gint64 g_get_monotonic_time(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (gint64) ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
