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

#include <ctype.h>

#include "shared.h"

/**
 * @brief Make `value` stepped as specified by `step`
 */

int32_t Step(int32_t value, int32_t step) {
	if (!step) {
		return 0; // divide by zero check
	}

	return (int32_t) floorf(value / (float) step) * step;
}

/**
 * @brief Returns a pseudo-random positive integer.
 *
 * Uses a Linear Congruence Generator, values kindly borrowed from glibc.
 * Look up the rules required for the constants before just replacing them;
 * performance is dictated by the selection.
 */
int32_t Random(void) {

	static uint32_t seed;
	static __thread uint32_t state = 0;
	static __thread _Bool uninitalized = true;

	if (uninitalized) {
		seed += (uint32_t) time(NULL);
		state = seed;
		uninitalized = false;
	}

	state = (1103515245 * state + 12345);
	return state & 0x7fffffff;
}

/**
 * @brief Returns a pseudo-random float between -1.0 and 1.0.
 */
float Randomc(void) {
	return (Random()) * (2.f / 0x7fffffff) - 1.f;
}

/**
 * @brief Returns a pseudo-random int32_t between min and max.
 */
int32_t Randomr(const int32_t min, const int32_t max) {
	return (int32_t) RandomRangef(min, max);
}

/**
 * @brief Handles wildcard suffixes for GlobMatch.
 */
static _Bool GlobMatchStar(const char *pattern, const char *text, const glob_flags_t flags) {
	const char *p = pattern, *t = text;
	register char c, c1;

	while ((c = *p++) == '?' || c == '*')
		if (c == '?' && *t++ == '\0') {
			return false;
		}

	if (c == '\0') {
		return true;
	}

	if (c == '\\') {
		c1 = *p;
	} else {
		c1 = c;
	}

	while (true) {
		if ((c == '[' || *t == c1) && GlobMatch(p - 1, t, flags)) {
			return true;
		}
		if (*t++ == '\0') {
			return false;
		}
	}

	return false;
}

/**
 * @brief Matches the pattern against specified text, returning true if the pattern
 * matches, false otherwise.
 *
 * A match means the entire string TEXT is used up in matching.
 *
 * In the pattern string, `*' matches any sequence of characters,
 * `?' matches any character, [SET] matches any character in the specified set,
 * [!SET] matches any character not in the specified set.
 *
 * A set is composed of characters or ranges; a range looks like
 * character hyphen character(as in 0-9 or A-Z).
 * [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
 * Any other character in the pattern must be matched exactly.
 *
 * To suppress the special syntactic significance of any of `[]*?!-\',
 * and match the character exactly, precede it with a `\'.
 */
_Bool GlobMatch(const char *pattern, const char *text, const glob_flags_t flags) {
	const char *p = pattern, *t = text;
	register char c;

	if (!p || !t) {
		return false;
	}

	while ((c = *p++) != '\0')
		switch (c) {
			case '?':
				if (*t == '\0') {
					return 0;
				} else {
					++t;
				}
				break;

			case '\\':
				if (*p++ != *t++) {
					return 0;
				}
				break;

			case '*':
				return GlobMatchStar(p, t, flags);

			case '[': {
					register char c1 = *t++;
					int32_t invert;

					if (!c1) {
						return 0;
					}

					invert = ((*p == '!') || (*p == '^'));
					if (invert) {
						p++;
					}

					c = *p++;
					while (true) {
						register char cstart = c, cend = c;

						if (c == '\\') {
							cstart = *p++;
							cend = cstart;
						}
						if (c == '\0') {
							return 0;
						}

						c = *p++;
						if (c == '-' && *p != ']') {
							cend = *p++;
							if (cend == '\\') {
								cend = *p++;
							}
							if (cend == '\0') {
								return 0;
							}
							c = *p++;
						}
						if (c1 >= cstart && c1 <= cend) {
							goto match;
						}
						if (c == ']') {
							break;
						}
					}
					if (!invert) {
						return 0;
					}
					break;

match:
					/* Skip the rest of the [...] construct that already matched. */
					while (c != ']') {
						if (c == '\0') {
							return 0;
						}
						c = *p++;
						if (c == '\0') {
							return 0;
						} else if (c == '\\') {
							++p;
						}
					}
					if (invert) {
						return 0;
					}
					break;
				}

			default:
				if (flags & GLOB_CASE_INSENSITIVE) {
					if (tolower(c) != tolower(*t++)) {
						return 0;
					}
				} else {
					if (c != *t++) {
						return 0;
					}
				}
				break;
		}

	return *t == '\0';
}

/**
 * @brief Returns the base name for the given file or path.
 */
const char *Basename(const char *path) {
	const char *last = path;
	while (*path) {
		if (*path == '/') {
			last = path + 1;
		}
		path++;
	}
	return last;
}

/**
 * @brief Returns the directory name for the given file or path name.
 */
void Dirname(const char *in, char *out) {
	char *c;

	if (!(c = strrchr(in, '/'))) {
		strcpy(out, "./");
		return;
	}

	while (in <= c) {
		*out++ = *in++;
	}

	*out = '\0';
}

/**
 * @brief Removes the first newline and everything following it
 * from the specified input string.
 */
void StripNewline(const char *in, char *out) {

	if (in) {
		const size_t len = strlen(in);
		memmove(out, in, len + 1);

		char *ext = strrchr(out, '\n');
		if (ext) {
			*ext = '\0';
		}
	} else {
		*out = '\0';
	}
}

/**
 * @brief Removes any file extension(s) from the specified input string.
 */
void StripExtension(const char *in, char *out) {

	if (in) {
		const size_t len = strlen(in);
		memmove(out, in, len + 1);

		char *ext = strrchr(out, '.');
		if (ext) {
			*ext = '\0';
		}
	} else {
		*out = '\0';
	}
}

/**
 * @return The color escape sequence at `c`, or -1 if none.
 */
_Bool StrIsColor(const char *c) {
	if (c) {
		if (*c == ESC_COLOR) {
			const char num = *(c + 1);
			if (num >= '0' && num <= '7') {
				return true;
			}
		}
	}
	return false;
}

/**
 * @return A color_t for the color specified escape sequence.
 */
color_t ColorEsc(int32_t esc) {
	switch (esc) {
		case ESC_COLOR_BLACK:
			return color_white;
		case ESC_COLOR_RED:
			return color_red;
		case ESC_COLOR_GREEN:
			return color_green;
		case ESC_COLOR_YELLOW:
			return color_yellow;
		case ESC_COLOR_BLUE:
			return color_blue;
		case ESC_COLOR_MAGENTA:
			return color_magenta;
		case ESC_COLOR_CYAN:
			return color_cyan;
		case ESC_COLOR_WHITE:
			return color_white;
	}

	return color_white;
}

/**
 * @brief Strips color escape sequences from the specified input string.
 */
void StripColors(const char *in, char *out) {

	while (*in) {

		if (StrIsColor(in)) {
			in += 2;
			continue;
		}

		*out++ = *in++;
	}
	*out = '\0';
}

/**
 * @brief Returns the length of s in printable characters.
 */
size_t StrColorLen(const char *s) {

	size_t len = 0;

	while (*s) {
		if (StrIsColor(s)) {
			s += 2;
			continue;
		}

		s++;
		len++;
	}

	return len;
}

/**
 * @brief Performs a color- and case-insensitive string comparison.
 */
int32_t StrColorCmp(const char *s1, const char *s2) {
	char string1[strlen(s1) + 1], string2[strlen(s2) + 1];

	StripColors(s1, string1);
	StripColors(s2, string2);

	return g_ascii_strcasecmp(string1, string2);
}

/**
 * @return The first color sequence in s.
 */
int32_t StrColor(const char *s) {

	const char *c = s;
	while (*c) {
		if (StrIsColor(c)) {
			return *(c + 1) - '0';
		}
		c++;
	}

	return ESC_COLOR_DEFAULT;
}

/**
 * @return The last occurrence of a color escape sequence in s.
 */
int32_t StrrColor(const char *s) {

	if (s) {
		const char *c = s + strlen(s) - 1;
		while (c > s) {
			if (StrIsColor(c)) {
				return *(c + 1) - '0';
			}
			c--;
		}
	}

	return ESC_COLOR_DEFAULT;
}

/**
 * @brief A shorthand g_snprintf into a statically allocated buffer. Several
 * buffers are maintained internally so that nested va()'s are safe within
 * reasonable limits. This function is not thread safe.
 */
char *va(const char *format, ...) {
	static char strings[8][MAX_STRING_CHARS];
	static uint16_t index;

	char *string = strings[index++ % 8];

	va_list args;

	va_start(args, format);
	vsnprintf(string, MAX_STRING_CHARS, format, args);
	va_end(args);

	return string;
}

/**
 * @brief A convenience function for printing vectors.
 */
char *vtos(const vec3_t v) {
	static uint32_t index;
	static char str[8][MAX_QPATH];

	char *s = str[index++ % 8];
	g_snprintf(s, MAX_QPATH, "(%4.2f %4.2f %4.2f)", v.x, v.y, v.z);

	return s;
}

/**
 * @brief Lowers an entire string.
 */
void StrLower(const char *in, char *out) {

	while (*in) {
		(*(out++)) = (char) tolower(*(in++));
	}
}

/**
 * @brief Searches the string for the given key and returns the associated value,
 * or an empty string.
 */
char *GetUserInfo(const char *s, const char *key) {
	char pkey[512];
	static char value[2][512]; // use two buffers so compares
	// work without stomping on each other
	static int32_t value_index;
	char *o;

	value_index ^= 1;
	if (*s == '\\') {
		s++;
	}
	while (true) {
		o = pkey;
		while (*s != '\\') {
			if (!*s) {
				return "";
			}
			*o++ = *s++;
		}
		*o = '\0';
		s++;

		o = value[value_index];

		while (*s != '\\' && *s) {
			if (!*s) {
				return "";
			}
			*o++ = *s++;
		}
		*o = '\0';

		if (!g_strcmp0(key, pkey)) {
			return value[value_index];
		}

		if (!*s) {
			break;
		}
		s++;
	}

	return "";
}

/**
 * @brief
 */
void DeleteUserInfo(char *s, const char *key) {
	char *start;
	char pkey[512];
	char value[512];
	char *o;

	if (strstr(key, "\\")) {
		return;
	}

	while (true) {
		start = s;
		if (*s == '\\') {
			s++;
		}
		o = pkey;
		while (*s != '\\') {
			if (!*s) {
				return;
			}
			*o++ = *s++;
		}
		*o = '\0';
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s) {
				return;
			}
			*o++ = *s++;
		}
		*o = '\0';

		if (!g_strcmp0(key, pkey)) {
			memmove(start, s, strlen(s) + 1);
			return;
		}

		if (!*s) {
			return;
		}
	}
}

/**
 * @brief Returns true if the specified user-info string appears valid, false
 * otherwise.
 */
_Bool ValidateUserInfo(const char *s) {
	if (!s || !*s) {
		return false;
	}
	if (strstr(s, "\"")) {
		return false;
	}
	if (strstr(s, ";")) {
		return false;
	}
	return true;
}

/**
 * @brief
 */
void SetUserInfo(char *s, const char *key, const char *value) {
	char newi[MAX_USER_INFO_STRING * 16], *v;

	if (strstr(key, "\\") || strstr(value, "\\")) {
		//Com_Print("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr(key, ";")) {
		//Com_Print("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr(key, "\"") || strstr(value, "\"")) {
		//Com_Print("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_USER_INFO_KEY - 1 || strlen(value) > MAX_USER_INFO_VALUE - 1) {
		//Com_Print("Keys and values must be < 64 characters\n");
		return;
	}

	DeleteUserInfo(s, key);

	if (!value || *value == '\0') {
		return;
	}

	g_snprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > MAX_USER_INFO_STRING * 16) {
		//Com_Print("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v) {
		char c = *v++;
		c &= 127; // strip high bits
		if (c >= 32 && c < 127) {
			*s++ = c;
		}
	}
	*s = '\0';
}

/**
 * @brief Case-insensitive version of g_str_equal
 */
gboolean g_stri_equal(gconstpointer v1, gconstpointer v2) {
	return g_ascii_strcasecmp((const gchar *) v1, (const gchar *) v2) == 0;
}

/**
 * @brief Case-insensitive version of g_str_hash
 */
guint g_stri_hash(gconstpointer v) {
	guint32 h = 5381;

	for (const char *p = (const char *) v; *p; p++) {
		h = (h << 5) + h + tolower(*p);
	}

	return h;
}
