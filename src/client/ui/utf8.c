/**
 * @file utf8.c
 */

/*
All original material Copyright (C) 2002-2007 UFO: Alien Invasion team.

Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "utf8.h"
#include <string.h>

/**
 * @brief Delete a whole (possibly multibyte) character from a string.
 * @param[in] s Start of the string
 * @param[in] pos Offset from the start
 * @return position where deleted character started
 */
int UTF8_delete_char (char *s, int pos)
{
	int start = pos;
	int next = pos;

	while (start > 0 && UTF8_CONTINUATION_BYTE(s[start]))
		start--;
	if (s[next] != 0)
		next++;
	while (s[next] != 0 && UTF8_CONTINUATION_BYTE(s[next]))
		next++;
	/* memmove is the only standard copying function that is guaranteed
	 * to work if the source and destination overlap. */
	memmove(&s[start], &s[next], strlen(&s[next]) + 1);
	return start;
}

/**
 * @brief Insert a (possibly multibyte) UTF-8 character into a string.
 * @param[in] s Start of the string
 * @param[in] n Buffer size of the string
 * @param[in] pos Offset from the start
 * @param[in] c Unicode code as 32-bit integer
 * @return Number of bytes added
 */
int UTF8_insert_char (char *s, int n, int pos, int c)
{
	const int utf8len = UTF8_encoded_len(c);
	const int tail = strlen(&s[pos]) + 1;

	if (utf8len == 0)
		return 0;

	if (pos + tail + utf8len >= n)
		return 0;

	/* Insertion: move up rest of string. Also moves string terminator. */
	memmove(&s[pos + utf8len], &s[pos], tail);

	if (c <= 0x7f) {
		s[pos] = c;
	} else if (c <= 0x7ff) { 				/* c has 11 bits */
		s[pos] = 0xc0 | (c >> 6);	  			/* high 5 bits */
		s[pos + 1] = 0x80 | (c & 0x3f); 		/* low 6 bits */
	} else if (c <= 0xffff) { 				/* c has 16 bits */
		s[pos] = 0xe0 | (c >> 12);				/* high 4 bits */
		s[pos + 1] = 0x80 | ((c >> 6) & 0x3f);	/* mid 6 bits */
		s[pos + 2] = 0x80 | (c & 0x3f);			/* low 6 bits */
	} else if (c <= 0x10ffff) {				/* c has 21 bits */
		s[pos] = 0xf0 | (c >> 18);				/* high 3 bits */
		s[pos + 1] = 0x80 | ((c >> 12) & 0x3f);	/* mid 6 bits */
		s[pos + 2] = 0x80 | ((c >> 6) & 0x3f);	/* mid 6 bits */
		s[pos + 3] = 0x80 | (c & 0x3f);			/* low 6 bits */
	}

	return utf8len;
}

/**
 * @brief length of UTF-8 character starting with this byte.
 * @return length of character encoding, or 0 if not start of a UTF-8 sequence
 * @todo Using this does not solve the truncation problem in case of
 * decomposed characters. For example a code for "a" followed by a
 * code for "put dots above previous character: the "a" will be reported
 * as a character of length 1 by this function, even though the code
 * that follows is part of its visual appearance and should not be
 * cut off separately. Fortunately decomposed characters are rarely used.
 */
int UTF8_char_len (unsigned char c)
{
	if (c < 0x80)
		return 1;
	if (c < 0xc0)
		return 0;
	if (c < 0xe0)
		return 2;
	if (c < 0xf0)
		return 3;
	if (c < 0xf8)
		return 4;
	/* UTF-8 used to define 5 and 6 byte sequences, but they are
	 * no longer valid. */
	return 0;
}

/**
 * Calculate how long a Unicode code point (such as returned by
 * SDL key events in unicode mode) would be in UTF-8 encoding.
 */
int UTF8_encoded_len (int c)
{
	if (c <= 0x7F)
		return 1;
	if (c <= 0x07FF)
		return 2;
	if (c <= 0xFFFF)
		return 3;
	if (c <= 0x10FFFF)  /* highest defined Unicode code */
		return 4;
	return 0;
}

/**
 * @brief Count the number of character (not the number of bytes) of a zero termination string
 * @note the '\0' termination characted is not counted
 * @note to count the number of bytes, use strlen
 * @sa strlen
 */
size_t UTF8_strlen (const char *str)
{
	size_t result = 0;

	while (str[0] != '\0') {
		const int n = UTF8_char_len((unsigned char)*str);
		str += n;
		result++;
	}
	return result;
}
