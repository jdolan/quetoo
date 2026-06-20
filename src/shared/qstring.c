/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "qstring.h"

/**
 * @see qstring.h
 */
size_t q_strlcpy(char *dst, const char *src, size_t size) {

	if (!src) {
		if (size) {
			dst[0] = '\0';
		}
		return 0;
	}

	const size_t src_len = strlen(src);

	if (size) {
		const size_t copy = src_len < size - 1 ? src_len : size - 1;
		memcpy(dst, src, copy);
		dst[copy] = '\0';
	}

	return src_len;
}

/**
 * @see qstring.h
 */
size_t q_strlcat(char *dst, const char *src, size_t size) {

	if (!src) {
		return strlen(dst);
	}

	const size_t dst_len = strlen(dst);
	const size_t src_len = strlen(src);

	if (dst_len < size) {
		const size_t copy = src_len < size - dst_len - 1 ? src_len : size - dst_len - 1;
		memcpy(dst + dst_len, src, copy);
		dst[dst_len + copy] = '\0';
	}

	return dst_len + src_len;
}

/**
 * @see qstring.h
 */
int32_t q_strcasecmp(const char *a, const char *b) {

	if (a == b) {
		return 0;
	}
	if (!a) {
		return -1;
	}
	if (!b) {
		return 1;
	}

	while (*a && *b) {
		const int32_t d = tolower((unsigned char) *a) - tolower((unsigned char) *b);
		if (d) {
			return d;
		}
		a++;
		b++;
	}

	return tolower((unsigned char) *a) - tolower((unsigned char) *b);
}

/**
 * @see qstring.h
 */
int32_t q_strncasecmp(const char *a, const char *b, size_t n) {

	if (a == b || n == 0) {
		return 0;
	}
	if (!a) {
		return -1;
	}
	if (!b) {
		return 1;
	}

	while (n-- && *a && *b) {
		const int32_t d = tolower((unsigned char) *a) - tolower((unsigned char) *b);
		if (d) {
			return d;
		}
		a++;
		b++;
		if (!n) {
			return 0;
		}
	}

	return n ? tolower((unsigned char) *a) - tolower((unsigned char) *b) : 0;
}

/**
 * @see qstring.h
 */
char *q_strdup(const char *s) {

	if (!s) {
		return NULL;
	}

	const size_t len = strlen(s) + 1;
	char *dup = malloc(len);
	if (dup) {
		memcpy(dup, s, len);
	}
	return dup;
}

/**
 * @see qstring.h
 */
char *q_strndup(const char *s, size_t n) {

	if (!s) {
		return NULL;
	}

	size_t len = 0;
	while (len < n && s[len]) {
		len++;
	}

	char *dup = malloc(len + 1);
	if (dup) {
		memcpy(dup, s, len);
		dup[len] = '\0';
	}
	return dup;
}

/**
 * @see qstring.h
 */
char *q_strtok_r(char *s, const char *delim, char **save_ptr) {
#if defined(_MSC_VER)
	return strtok_s(s, delim, save_ptr);
#else
	return strtok_r(s, delim, save_ptr);
#endif
}

/**
 * @see qstring.h
 */
void q_strlower(const char *in, char *out) {

	if (in) {
		while (*in) {
			*out++ = (char) tolower((unsigned char) *in++);
		}
	}
	*out = '\0';
}

/**
 * @see qstring.h
 */
void q_strtrim(const char *in, char *out) {

	if (!in) {
		*out = '\0';
		return;
	}

	while (*in == ' ' || *in == '\t' || *in == '\r' || *in == '\n') {
		in++;
	}

	const char *end = in + strlen(in);
	while (end > in && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\r' || *(end - 1) == '\n')) {
		end--;
	}

	const size_t len = (size_t)(end - in);
	memmove(out, in, len);
	out[len] = '\0';
}

/**
 * @see qstring.h
 */
void q_strcolorstrip(const char *in, char *out) {

	while (*in) {
		if (q_striscolor(in)) {
			in += 2;
			continue;
		}
		*out++ = *in++;
	}
	*out = '\0';
}

/**
 * @see qstring.h
 */
size_t q_strcolorlen(const char *s) {

	size_t len = 0;

	while (s && *s) {
		if (q_striscolor(s)) {
			s += 2;
			continue;
		}
		s++;
		len++;
	}

	return len;
}

/**
 * @see qstring.h
 */
int32_t q_strcolorcmp(const char *s1, const char *s2) {

	char a[q_strlen(s1) + 1], b[q_strlen(s2) + 1];

	q_strcolorstrip(s1 ? s1 : "", a);
	q_strcolorstrip(s2 ? s2 : "", b);

	return q_strcasecmp(a, b);
}

/**
 * @see qstring.h
 */
int32_t q_strcolor(const char *s) {

	const char *c = s;
	while (c && *c) {
		if (q_striscolor(c)) {
			return *(c + 1) - '0';
		}
		c++;
	}

	return ESC_COLOR_DEFAULT;
}

/**
 * @see qstring.h
 */
int32_t q_strrcolor(const char *s) {

	if (s) {
		const char *c = s + strlen(s) - 1;
		while (c > s) {
			if (q_striscolor(c)) {
				return *(c + 1) - '0';
			}
			c--;
		}
	}

	return ESC_COLOR_DEFAULT;
}
