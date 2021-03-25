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

#include "parse.h"

/**
 * @brief Initialize a parser with the specified data and flags.
 */
void Parse_Init(parser_t *parser, const char *data, const parser_flags_t flags) {

	memset(parser, 0, sizeof(*parser));

	parser->flags = flags;
	parser->start = data;
	parser->position.ptr = data;
}

/**
 * @brief Return true if the parser is at the end of the input.
 */
_Bool Parse_IsEOF(const parser_t *parser) {
	return (*parser->position.ptr) == '\0';
}

/**
 * @brief Return true if the parser is at a newline boundary.
 */
_Bool Parse_IsEOL(const parser_t *parser) {
	const char c = *parser->position.ptr;
	return c == '\r' || c == '\n';
}

/**
 * @brief Trigger a column increase
 */
static void Parse_NextColumn(parser_t *parser, const size_t len) {
	parser->position.col += len;
}

/**
 * @brief Trigger a row increase
 */
static void Parse_NextRow(parser_t *parser, const size_t len) {
	parser->position.row += len;
	parser->position.col = 0;
}

/**
 * @brief Attempt to skip whitespace and find the start of a new token. The cursor will either be positioned
 * at the start of a non-control character or at a newline if flags tell them not to traverse them.
 */
static _Bool Parse_SkipWhitespace(parser_t *parser, const parse_flags_t flags) {
	char c;

	while ((c = *parser->position.ptr) <= ' ') {

		// end of parse
		if (c == '\0') {
			return false;
		}

		// see if we shouldn't traverse newlines
		if (c == '\r' || c == '\n') {
			if (flags & PARSE_NO_WRAP) {
				return false;
			}

			if (c == '\n') {
				Parse_NextRow(parser, 1);
			}
		}

		parser->position.ptr++;
		Parse_NextColumn(parser, 1);
	}

	// made it!
	return true;
}

/**
 * @brief Attempt to parse and skip a line comment that begins with the specified identifier.
 * Returns true if we found any comments.
 */
static _Bool Parse_SkipCommentLine(parser_t *parser, const char *identifier) {

	if (strncmp(parser->position.ptr, identifier, strlen(identifier))) {
		return false;
	}

	parser->position.ptr += strlen(identifier);
	Parse_NextColumn(parser, 2);

	while (true) {
		char c = *parser->position.ptr;

		if (c == '\0') {
			return false;
		}

		size_t skipped = 0;

		while (c == '\r' || c == '\n') {

			if (c == '\n') {
				skipped++; // reached one!
			}

			c = *(++parser->position.ptr);
		}

		if (skipped) {
			Parse_NextRow(parser, skipped);
			return true;
		}

		parser->position.ptr++;
		Parse_NextColumn(parser, 1);
	}

	return false;
}

/**
 * @brief Attempt to parse and skip a block comment that begins with the specified identifier.
 * Returns true if we found any comments.
 */
static _Bool Parse_SkipCommentBlock(parser_t *parser, const char *start, const char *end) {

	if (strncmp(parser->position.ptr, start, strlen(start))) {
		return false;
	}

	parser->position.ptr += strlen(start);
	Parse_NextColumn(parser, strlen(start));

	while (true) {
		char c = *parser->position.ptr;

		if (c == '\0') {
			return false;
		}

		if (!strncmp(parser->position.ptr, end, strlen(end))) {
			parser->position.ptr += strlen(end); // found it!
			Parse_NextColumn(parser, strlen(end));
			return true;
		}

		parser->position.ptr++;

		if (c == '\n') {
			Parse_NextRow(parser, 1);
		} else {
			Parse_NextColumn(parser, 1);
		}
	}

	return false;
}

/**
 * @brief Attempts to skip any comments that may be at the start of this token. This function should
 * only be called once the start of a token has been established.
 * @returns false if we are at EOF
 */
static _Bool Parse_SkipComments(parser_t *parser) {

	while (true) {
		char c = *parser->position.ptr;
		_Bool parsed_comments = false;

		if (c == '/') {

			if (!parsed_comments && (parser->flags & PARSER_C_LINE_COMMENTS)) {
				parsed_comments = Parse_SkipCommentLine(parser, "//") || parsed_comments;
			}

			if (!parsed_comments && (parser->flags & PARSER_C_BLOCK_COMMENTS)) {
				parsed_comments = Parse_SkipCommentBlock(parser, "/*", "*/") || parsed_comments;
			}
		} else if (c == '#') {

			if (!parsed_comments && (parser->flags & PARSER_POUND_LINE_COMMENTS)) {
				parsed_comments = Parse_SkipCommentLine(parser, "#") || parsed_comments;
			}
		}

		if (!parsed_comments) {
			break;
		}

		if (!Parse_SkipWhitespace(parser, PARSE_DEFAULT)) {
			return false;
		}
	}

	return !Parse_IsEOF(parser);
}

/**
 * @brief Handles the appending routine for output. Returns false if the added character would overflow the
 * output buffer.
 */
static _Bool Parse_AppendOutputChar(parser_t *parser, const parse_flags_t flags, const char c, size_t *output_position, char *output, const size_t output_len) {

	if (!output) {
		return true;
	}

	if (*output_position >= output_len - 1) { // buffer overrun
		if (!(flags & PARSE_ALLOW_OVERRUN)) {
			return false;
		}
	} else {
		output[(*output_position)++] = c;
	}

	return true;
}

/**
 * @brief Handles parsing a quoted string.
 */
static _Bool Parse_ParseQuotedString(parser_t *parser, const parse_flags_t flags, size_t *output_position, char *output, const size_t output_len) {
	char c = *parser->position.ptr;

	if (c != '"') {
		return false; // sanity check
	}

	if (flags & PARSE_RETAIN_QUOTES) {
		if (!Parse_AppendOutputChar(parser, flags, '"', output_position, output, output_len)) {
			return false;
		}
	}

	while (true) {
		c = *(++parser->position.ptr);
		Parse_NextColumn(parser, 1);

		if (c == '\0') {
			return false;
		} else if (c == '\\') {

			if (!(flags & PARSE_COPY_QUOTED_LITERALS)) {
				// not copying literally, so let's parse the value we want
				const char n = *(parser->position.ptr + 1);
				char escaped;

				switch (n) {
				default:
					escaped = '\0';
					break;
				// requires special char
				case 'n':
					escaped = '\n';
					break;
				case 't':
					escaped = '\t';
					break;
				// same as input char
				case '"':
				case '\'':
				case '\\':
					escaped = n;
					break;
				}

				if (escaped != '\0') {

					// copy it in
					if (!Parse_AppendOutputChar(parser, flags, escaped, output_position, output, output_len)) {
						return false;
					}

					parser->position.ptr++; // skip the next one since we're valid and parsed it above
					Parse_NextColumn(parser, 1);
					continue; // go right from after this bit
				}
			}

			// if we reached here, we're copying them literally or was an invalid escape sequence.
			if (!Parse_AppendOutputChar(parser, flags, c, output_position, output, output_len) ||
				!Parse_AppendOutputChar(parser, flags, *(++parser->position.ptr), output_position, output, output_len)) {
				return false;
			}

			Parse_NextColumn(parser, 1);
			continue; // go to next char
		} else if (c == '"') {
			// eat the char and we're done!
			parser->position.ptr++;
			Parse_NextColumn(parser, 1);
			break;
		} else if (c == '\n') {
			Parse_NextRow(parser, 1);
		}

		// regular char, just append
		if (!Parse_AppendOutputChar(parser, flags, c, output_position, output, output_len)) {
			return false;
		}
	}

	if (flags & PARSE_RETAIN_QUOTES) {
		if (!Parse_AppendOutputChar(parser, flags, '"', output_position, output, output_len)) {
			return false;
		}
	}

	return true;
}

/**
 * @brief Parse a single token out of the parser with the specified parse flags. You must pass your own
 * buffer into this function.
 * @returns false if the token cannot fit in the specified buffer, true if the parsing has succeeded.
 */
_Bool Parse_Token(parser_t *parser, const parse_flags_t flags, char *output, const size_t output_len) {
	parser_position_t old_position = { NULL, 0, 0 };

	if (!parser) {
		return false;
	}

	if (flags & PARSE_PEEK) {
		old_position = parser->position;
	}

	// empty out da token
	if (output) {

		if (!output_len) {
			return false; // why did you do this
		}

		output[0] = '\0';
	}

	// nothing to parse
	if (!parser->start) {
		return false;
	}

	// start by skipping whitespace tokens
	if (!Parse_SkipWhitespace(parser, flags)) {
		return false;
	}

	// check comments
	if (!Parse_SkipComments(parser)) {
		return false;
	}

	// now we're at the beginning of a token
	// start parsing!
	char c = *parser->position.ptr;
	size_t i = 0;

	if (c == '"') { // handle quotes with special function

		if (!Parse_ParseQuotedString(parser, flags, &i, output, output_len)) {
			return false;
		}

	} else {
		// regular token
		while (c > 32) {

			if (!Parse_AppendOutputChar(parser, flags, c, &i, output, output_len)) {
				return false;
			}

			c = *(++parser->position.ptr);
			Parse_NextColumn(parser, 1);
		}
	}

	if (!Parse_AppendOutputChar(parser, flags, '\0', &i, output, output_len)) {
		return false;
	}

	if (flags & PARSE_PEEK) {
		parser->position = old_position;
	}

	return true;
}

/**
 * @brief Get byte size for parse_type_t
 */
static size_t Parse_TypeSize(const parse_type_t type) {

	switch (type) {
	case PARSE_UINT8:
	case PARSE_INT8:
		return 1;
	case PARSE_UINT16:
	case PARSE_INT16:
		return 2;
	case PARSE_UINT32:
	case PARSE_INT32:
	case PARSE_FLOAT:
		return 4;
	case PARSE_DOUBLE:
		return 8;
	default:
		signal(SIGSEGV, NULL);
		return 0;
	}
}
/**
 * @brief Parse the specified data type.
 */
static _Bool Parse_TypeParse(const parse_type_t type, const char *input, void *output) {
	int32_t result;
	static byte scan_buffer[sizeof(double)];
	const size_t type_size = Parse_TypeSize(type);

	switch (type) {
	case PARSE_UINT8:
	case PARSE_UINT16:
	case PARSE_UINT32:
		result = sscanf(input, "%" SCNu32, (uint32_t *) scan_buffer);
		break;
	case PARSE_INT8:
	case PARSE_INT16:
	case PARSE_INT32:
		result = sscanf(input, "%" SCNi32, (int32_t *) scan_buffer);
		break;
	case PARSE_FLOAT:
		result = sscanf(input, "%f", (float *) scan_buffer);
		break;
	case PARSE_DOUBLE:
		result = sscanf(input, "%lf", (double *) scan_buffer);
		break;
	default:
		result = 0;
		signal(SIGSEGV, NULL);
	}

	if (result == 1) {
		if (output) {
			memcpy(output, scan_buffer, type_size);
		}
		return true;
	}

	return false;
}

/**
 * @brief Parse typed data out of the parser with the specified parse flags. You may pass NULL as the output
 * if you only wish to verify that the data can be parsed and not actually store the results.
 * @returns false if the specified data type cannot be parsed from the specified position in the parser.
 */
size_t Parse_Primitive(parser_t *parser, const parse_flags_t flags, const parse_type_t type, void *output, const size_t count) {
	parser_position_t old_position = { NULL, 0, 0 };
	const size_t type_size = Parse_TypeSize(type);
	size_t num_parsed = 0;

	if (flags & PARSE_PEEK) {
		old_position = parser->position;
	}

	const parse_flags_t prim_flags = ((flags & PARSE_WITHIN_QUOTES) ? (flags | PARSE_RETAIN_QUOTES) : flags) & ~PARSE_PEEK;

	if (!Parse_Token(parser, prim_flags, parser->scratch, sizeof(parser->scratch))) {

		if (flags & PARSE_PEEK) {
			parser->position = old_position;
		}

		return num_parsed;
	}

	// if we had quotes...
	if (*parser->scratch == '"' && (flags & PARSE_WITHIN_QUOTES)) {
		parser_t sub_parser;

		// init sub-parser without quotes
		parser->scratch[strlen(parser->scratch) - 1] = '\0';
		Parse_Init(&sub_parser, parser->scratch + 1, parser->flags);

		num_parsed = Parse_Primitive(&sub_parser, flags & ~(PARSE_WITHIN_QUOTES | PARSE_PEEK), type, output, count);
	} else {
		for (size_t i = 0; i < count; i++) {

			if (i != 0) { // 0 is parsed above for quote checking
				if (!Parse_Token(parser, prim_flags, parser->scratch, sizeof(parser->scratch))) {

					if (flags & PARSE_PEEK) {
						parser->position = old_position;
					}

					return num_parsed;
				}
			}

			if (!Parse_TypeParse(type, parser->scratch, output)) {
				break;
			}

			num_parsed++;

			if (output) {
				output = ((uint8_t *) output) + type_size;
			}
		}
	}

	if (flags & PARSE_PEEK) {
		parser->position = old_position;
	}

	return num_parsed;
}
