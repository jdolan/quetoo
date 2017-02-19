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

#include "shared.h"

/**
 * @brief Bitflags applied globally to the parser itself.
 */
typedef enum {
	/**
	 * @brief No comments allowed
	 */
	PARSER_NO_COMMENTS = 0,

	/**
	 * @brief Have C-style line comments // act as a comment (ignored by parser)
	 */
	PARSER_C_LINE_COMMENTS = 1,

	/**
	 * @brief Have C-style block comments / * * / act as comments (ignored by parser)
	 */
	PARSER_C_BLOCK_COMMENTS = 2,

	/**
	 * @brief Have pound line comments # act as a comment (ignored by parser)
	 */
	PARSER_POUND_LINE_COMMENTS = 4,

	/**
	 * @brief Default parser settings
	 */
	PARSER_DEFAULT = PARSER_C_LINE_COMMENTS,
} parser_flags_t;

/**
 * @brief State used to determine parser positioning
 */
typedef struct {
	const char *ptr;
	uint32_t row, col;
} parser_position_t;

/**
 * @brief A parser struct to be kept alive for the parsing procedure. Stores state required
 * by the parser routines to function properly.
 * @note Never modify this struct directly!
 */
typedef struct {
	// static members, only set on initialization
	parser_flags_t flags;
	const char *start;

	// dynamic members, change through parsing
	char scratch[3 + DBL_MANT_DIG - DBL_MIN_EXP + 1]; // enough to hold one full double plus \0
	parser_position_t position;
} parser_t;

/**
 * @brief Bitflags applied to a single parse routine call.
 */
typedef enum {
	/**
	 * @brief All characters in a quoted string are copied literally
	 */
	PARSE_COPY_QUOTED_LITERALS = 1,

	/**
	 * @brief Don't traverse line returns. This causes parses to return false and 
	 * Parse_EndOfLine to return true if we're waiting for a regular parse to move forward.
	 */
	PARSE_NO_WRAP = 2,

	/**
	 * @brief If the parsed primitives are contained within quotes, ignore the quotes.
	 */
	PARSE_WITHIN_QUOTES = 4,

	/**
	 * @brief Retain quotes that are parsed as part of a string.
	 */
	PARSE_RETAIN_QUOTES = 8,

	/**
	 * @brief Don't change our actual position
	 */
	PARSE_PEEK = 16,

	/**
	 * @brief Default parser settings
	 */
	PARSE_DEFAULT = PARSE_COPY_QUOTED_LITERALS
} parse_flags_t;

/**
 * @brief Types for Parser_Parse
 */
typedef enum {
	PARSE_UINT8,
	PARSE_INT8,
	PARSE_UINT16,
	PARSE_INT16,
	PARSE_UINT32,
	PARSE_INT32,
	PARSE_FLOAT,
	PARSE_DOUBLE
} parse_type_t;

void Parse_Init(parser_t *parser, const char *data, const parser_flags_t flags);
_Bool Parse_IsEOF(const parser_t *parser);
_Bool Parse_IsEOL(const parser_t *parser);
_Bool Parse_Token(parser_t *parser, const parse_flags_t flags, char *output, const size_t output_len);
size_t Parse_Primitive(parser_t *parser, const parse_flags_t flags, const parse_type_t type, void *output, const size_t count);

#define Parse_SkipToken(parser, flags) \
		Parse_Token(parser, flags, NULL, 0)

#define Parse_SkipPrimitive(parser, flags, type, count) \
		Parse_Primitive(parser, flags, type, NULL, count)

#define Parse_PeekToken(parser, flags, output, output_len) \
		Parse_Token(parser, flags | PARSE_PEEK, output, output_len)

#define Parse_PeekPrimitive(parser, flags, type, output, count) \
		Parse_Primitive(parser, flags | PARSE_PEEK, type, output, count)