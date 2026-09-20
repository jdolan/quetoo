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

#include <signal.h>
#include "parse.h"

/**
 * @brief Return true if the parser is at the end of the input.
 */
bool Parse_IsEOF(const Parser *parser) {
  return (*parser->position.ptr) == '\0';
}

/**
 * @brief Return true if the parser is at a newline boundary.
 */
bool Parse_IsEOL(const Parser *parser) {
  const char c = *parser->position.ptr;
  return c == '\r' || c == '\n';
}

/**
 * @brief Trigger a column increase
 */
static void Parse_NextColumn(Parser *parser, const size_t len) {
  parser->position.col += len;
}

/**
 * @brief Trigger a row increase
 */
static void Parse_NextRow(Parser *parser, const size_t len) {
  parser->position.row += len;
  parser->position.col = 0;
}

/**
 * @brief Attempt to skip whitespace and find the start of a new token. The cursor will either be positioned
 * at the start of a non-control character or at a newline if flags tell them not to traverse them.
 */
static bool Parse_SkipWhitespace(Parser *parser, const ParseFlags flags) {
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
static bool Parse_SkipCommentLine(Parser *parser, const char *identifier) {

  if (q_strncmp(parser->position.ptr, identifier, q_strlen(identifier))) {
    return false;
  }

  parser->position.ptr += q_strlen(identifier);
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
static bool Parse_SkipCommentBlock(Parser *parser, const char *start, const char *end) {

  if (q_strncmp(parser->position.ptr, start, q_strlen(start))) {
    return false;
  }

  parser->position.ptr += q_strlen(start);
  Parse_NextColumn(parser, q_strlen(start));

  while (true) {
    char c = *parser->position.ptr;

    if (c == '\0') {
      return false;
    }

    if (!q_strncmp(parser->position.ptr, end, q_strlen(end))) {
      parser->position.ptr += q_strlen(end); // found it!
      Parse_NextColumn(parser, q_strlen(end));
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
 * @return false if we are at EOF
 */
static bool Parse_SkipComments(Parser *parser) {

  while (true) {
    char c = *parser->position.ptr;
    bool parsedComments = false;

    if (c == '/') {

      if (!parsedComments && (parser->flags & PARSER_C_LINE_COMMENTS)) {
        parsedComments = Parse_SkipCommentLine(parser, "//") || parsedComments;
      }

      if (!parsedComments && (parser->flags & PARSER_C_BLOCK_COMMENTS)) {
        parsedComments = Parse_SkipCommentBlock(parser, "/*", "*/") || parsedComments;
      }
    } else if (c == '#') {

      if (!parsedComments && (parser->flags & PARSER_POUND_LINE_COMMENTS)) {
        parsedComments = Parse_SkipCommentLine(parser, "#") || parsedComments;
      }
    }

    if (!parsedComments) {
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
static bool Parse_AppendOutputChar(Parser *parser, const ParseFlags flags, const char c, size_t *outputPosition, char *output, const size_t outputLen) {

  if (!output) {
    return true;
  }

  if (*outputPosition >= outputLen - 1) { // buffer overrun
    if (!(flags & PARSE_ALLOW_OVERRUN)) {
      return false;
    }
  } else {
    output[(*outputPosition)++] = c;
  }

  return true;
}

/**
 * @brief Handles parsing a quoted string.
 */
static bool Parse_ParseQuotedString(Parser *parser, const ParseFlags flags, size_t *outputPosition, char *output, const size_t outputLen) {
  char c = *parser->position.ptr;

  if (c != '"') {
    return false; // sanity check
  }

  if (flags & PARSE_RETAIN_QUOTES) {
    if (!Parse_AppendOutputChar(parser, flags, '"', outputPosition, output, outputLen)) {
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
          if (!Parse_AppendOutputChar(parser, flags, escaped, outputPosition, output, outputLen)) {
            return false;
          }

          parser->position.ptr++; // skip the next one since we're valid and parsed it above
          Parse_NextColumn(parser, 1);
          continue; // go right from after this bit
        }
      }

      // if we reached here, we're copying them literally or was an invalid escape sequence.
      ++parser->position.ptr;
      if (*parser->position.ptr == '\0') {
        return false;
      }
      if (!Parse_AppendOutputChar(parser, flags, c, outputPosition, output, outputLen) ||
        !Parse_AppendOutputChar(parser, flags, *parser->position.ptr, outputPosition, output, outputLen)) {
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
    if (!Parse_AppendOutputChar(parser, flags, c, outputPosition, output, outputLen)) {
      return false;
    }
  }

  if (flags & PARSE_RETAIN_QUOTES) {
    if (!Parse_AppendOutputChar(parser, flags, '"', outputPosition, output, outputLen)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Parse a single token out of the parser with the specified parse flags. You must pass your own
 * buffer into this function.
 * @return false if the token cannot fit in the specified buffer, true if the parsing has succeeded.
 */
bool Parse_Token(Parser *parser, const ParseFlags flags, char *output, const size_t outputLen) {
  ParserPosition oldPosition = { NULL, 0, 0 };

  if (!parser) {
    return false;
  }

  if (flags & PARSE_PEEK) {
    oldPosition = parser->position;
  }

  // empty out da token
  if (output) {

    if (!outputLen) {
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

    if (!Parse_ParseQuotedString(parser, flags, &i, output, outputLen)) {
      return false;
    }

  } else {
    // regular token
    while (c > 32) {

      if (!Parse_AppendOutputChar(parser, flags, c, &i, output, outputLen)) {
        return false;
      }

      c = *(++parser->position.ptr);
      Parse_NextColumn(parser, 1);
    }
  }

  if (!Parse_AppendOutputChar(parser, flags, '\0', &i, output, outputLen)) {
    return false;
  }

  if (flags & PARSE_PEEK) {
    parser->position = oldPosition;
  }

  return true;
}

/**
 * @brief Get byte size for `ParseType`
 */
static size_t Parse_TypeSize(const ParseType type) {

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
static bool Parse_TypeParse(const ParseType type, const char *input, void *output) {
  int32_t result;
  static byte scan_buffer[sizeof(double)];
  const size_t typeSize = Parse_TypeSize(type);

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
    if (isinf(*(float *) scan_buffer) || isnan(*(float *) scan_buffer)) {
      result = 0;
    }
    break;
  case PARSE_DOUBLE:
    result = sscanf(input, "%lf", (double *) scan_buffer);
    if (isinf(*(double *) scan_buffer) || isnan(*(double *) scan_buffer)) {
      result = 0;
    }
    break;
  default:
    result = 0;
    signal(SIGSEGV, NULL);
  }

  if (result == 1) {
    if (output) {
      memcpy(output, scan_buffer, typeSize);
    }
    return true;
  }

  return false;
}

static __thread char scratch[3 + DBL_MANT_DIG - DBL_MIN_EXP + 1]; // enough to hold one full double plus \0

/**
 * @brief Parse typed data out of the parser with the specified parse flags. You may pass `NULL` as the output
 * if you only wish to verify that the data can be parsed and not actually store the results.
 * @return The number of primitives successfully parsed.
 */
size_t Parse_Primitive(Parser *parser, const ParseFlags flags, const ParseType type, void *output, const size_t count) {
  ParserPosition oldPosition = { NULL, 0, 0 };
  const size_t typeSize = Parse_TypeSize(type);
  size_t numParsed = 0;

  if (flags & PARSE_PEEK) {
    oldPosition = parser->position;
  }

  const ParseFlags primFlags = ((flags & PARSE_WITHIN_QUOTES) ? (flags | PARSE_RETAIN_QUOTES) : flags) & ~PARSE_PEEK;

  if (!Parse_Token(parser, primFlags, scratch, sizeof(scratch))) {

    if (flags & PARSE_PEEK) {
      parser->position = oldPosition;
    }

    return numParsed;
  }

  // if we had quotes...
  if (*scratch == '"' && (flags & PARSE_WITHIN_QUOTES)) {
    // init sub-parser without quotes
    scratch[q_strlen(scratch) - 1] = '\0';

    numParsed = Parse_QuickPrimitive(scratch + 1, parser->flags, flags & ~(PARSE_WITHIN_QUOTES | PARSE_PEEK), type, output, count);
  } else {
    for (size_t i = 0; i < count; i++) {

      if (i != 0) { // 0 is parsed above for quote checking
        if (!Parse_Token(parser, primFlags, scratch, sizeof(scratch))) {

          if (flags & PARSE_PEEK) {
            parser->position = oldPosition;
          }

          return numParsed;
        }
      }

      if (!Parse_TypeParse(type, scratch, output)) {
        break;
      }

      numParsed++;

      if (output) {
        output = ((uint8_t *) output) + typeSize;
      }
    }
  }

  if (flags & PARSE_PEEK) {
    parser->position = oldPosition;
  }

  return numParsed;
}
