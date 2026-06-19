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

#pragma once

#include "quetoo.h"

/**
 * @file qstring.h
 * @brief Portable, null-safe string utilities.
 *
 * Every string operation in the codebase should use a q_str* function from
 * this header rather than its raw C, POSIX, or SDL equivalent. This gives us:
 *
 *  - Portability: q_strlcpy/q_strlcat work on all platforms; q_strcasecmp
 *    does not rely on locale-sensitive POSIX functions; q_strtok_r maps to
 *    strtok_s on MSVC.
 *
 *  - Null-safety: every comparison, search, and length function treats a NULL
 *    argument as an empty string rather than crashing.
 *
 *  - Allocation consistency: q_strdup/q_strndup use malloc; the caller frees
 *    with free(). No SDL_free / Mem_Free ambiguity.
 */

/**
 * @brief Portable snprintf. snprintf is C99 and available everywhere.
 */
#define q_snprintf snprintf

/**
 * @return The length of `s`, or 0 if `s` is NULL.
 */
static inline size_t __attribute__ ((warn_unused_result)) q_strlen(const char *s) {
	return s ? strlen(s) : 0;
}

/**
 * @return The first occurrence of `c` in `s`, or NULL if not found or `s` is NULL.
 */
static inline const char * __attribute__ ((warn_unused_result)) q_strchr(const char *s, int c) {
	return s ? strchr(s, c) : NULL;
}

/**
 * @return The last occurrence of `c` in `s`, or NULL if not found or `s` is NULL.
 */
static inline const char * __attribute__ ((warn_unused_result)) q_strrchr(const char *s, int c) {
	return s ? strrchr(s, c) : NULL;
}

/**
 * @return The first occurrence of `needle` in `haystack`, or NULL if either is NULL.
 */
static inline const char * __attribute__ ((warn_unused_result)) q_strstr(const char *haystack, const char *needle) {
	return (haystack && needle) ? strstr(haystack, needle) : NULL;
}

/**
 * @return Negative if `a` < `b`, 0 if equal, positive if `a` > `b`.
 * NULL is ordered before any non-NULL string; NULL == NULL.
 */
static inline int32_t __attribute__ ((warn_unused_result)) q_strcmp(const char *a, const char *b) {
	if (a == b) {
		return 0;
	}
	if (!a) {
		return -1;
	}
	if (!b) {
		return 1;
	}
	return strcmp(a, b);
}

/**
 * @return Compares at most `n` characters. NULL-safe.
 */
static inline int32_t __attribute__ ((warn_unused_result)) q_strncmp(const char *a, const char *b, size_t n) {
	if (a == b || n == 0) {
		return 0;
	}
	if (!a) {
		return -1;
	}
	if (!b) {
		return 1;
	}
	return strncmp(a, b, n);
}

/**
 * @return True if `s` begins with `prefix`. NULL-safe.
 */
static inline bool __attribute__ ((warn_unused_result)) q_str_has_prefix(const char *s, const char *prefix) {
	return (s && prefix) ? strncmp(s, prefix, strlen(prefix)) == 0 : false;
}

/**
 * @return True if `s` ends with `suffix`. NULL-safe.
 */
static inline bool __attribute__ ((warn_unused_result)) q_str_has_suffix(const char *s, const char *suffix) {
	if (!s || !suffix) {
		return false;
	}
	const size_t slen = strlen(s);
	const size_t sufflen = strlen(suffix);
	return sufflen <= slen && strcmp(s + slen - sufflen, suffix) == 0;
}

/**
 * @brief Copies up to `size - 1` characters from `src` to `dst`, always
 * NUL-terminating. If `src` is NULL, `dst` is set to "".
 * @return The length of `src` (not the number of bytes written).
 */
size_t q_strlcpy(char *dst, const char *src, size_t size);

/**
 * @brief Appends `src` to `dst`, writing at most `size - strlen(dst) - 1`
 * bytes, always NUL-terminating. If `src` is NULL, `dst` is unchanged.
 * @return The total length that would result if `size` were unlimited.
 */
size_t q_strlcat(char *dst, const char *src, size_t size);

/**
 * @brief Case-insensitive string comparison. NULL-safe.
 * @return Negative if `a` < `b`, 0 if equal, positive if `a` > `b`.
 */
int32_t __attribute__ ((warn_unused_result)) q_strcasecmp(const char *a, const char *b);

/**
 * @brief Case-insensitive comparison of at most `n` characters. NULL-safe.
 */
int32_t __attribute__ ((warn_unused_result)) q_strncasecmp(const char *a, const char *b, size_t n);

/**
 * @brief Null-safe strdup using malloc. The caller must free() the result.
 * @return A heap copy of `s`, or NULL if `s` is NULL.
 */
char * __attribute__ ((warn_unused_result)) q_strdup(const char *s);

/**
 * @brief Portable strndup using malloc. Copies at most `n` characters,
 * always NUL-terminates. The caller must free() the result.
 * @return A heap copy, or NULL if `s` is NULL.
 */
char * __attribute__ ((warn_unused_result)) q_strndup(const char *s, size_t n);

/**
 * @brief Portable reentrant tokenizer. Uses strtok_s on MSVC.
 */
char *q_strtok_r(char *s, const char *delim, char **save_ptr);
