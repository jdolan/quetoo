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
 * @brief Handles wildcard suffixes for GlobMatch.
 */
static bool GlobMatchStar(const char *pattern, const char *text, const glob_flags_t flags) {
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
 * In the pattern string, `*` matches any sequence of characters,
 * `?` matches any character, [SET] matches any character in the specified set,
 * [!SET] matches any character not in the specified set.
 *
 * A set is composed of characters or ranges; a range looks like
 * character hyphen character(as in 0-9 or A-Z).
 * `[0-9a-zA-Z_]` is the set of characters allowed in C identifiers.
 * Any other character in the pattern must be matched exactly.
 *
 * To suppress the special syntactic significance of any of []`*?!-\`,
 * and match the character exactly, precede it with a `\`.
 */
bool GlobMatch(const char *pattern, const char *text, const glob_flags_t flags) {
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

  if (!(c = q_strrchr(in, '/'))) {
    strcpy(out, "./");
    return;
  }

  while (in <= c) {
    *out++ = *in++;
  }

  *out = '\0';
}

/**
 * @brief Removes any file extension(s) from the specified input string.
 */
void StripExtension(const char *in, char *out) {

  if (in) {
    const size_t len = q_strlen(in);
    memmove(out, in, len + 1);

    char *ext = q_strrchr(out, '.');
    if (ext) {
      *ext = '\0';
    }
  } else {
    *out = '\0';
  }
}


/**
 * @return True if `c` is an emoji escape sequence, false otherwise.
 */
bool StrIsEmoji(const char *c) {
  if (c) {
    if (*c == ESC_EMOJI) {
      c++;
      if (isalpha(*c)) {
        while (isalnum(*c) || q_strchr("_-", *c)) {
          c++;
        }
        if (*c == ESC_EMOJI) {
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * @return A `color_t` for the color specified escape sequence.
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
    case ESC_COLOR_ORANGE:
      return color_orange;
    case ESC_COLOR_GREY:
      return color_grey;
  }

  return color_white;
}

/**
 * @brief Extracts the emoji sequence at `in` to `out`, returning a pointer to the next character
 * after the emoji sequence.
 */
const char *EmojiEsc(const char *in, char *out, size_t out_size) {

  assert(*in == ESC_EMOJI);

  in++;

  for (size_t i = 0; i < out_size - 1; i++) {
    if (isalnum(*in) || q_strchr("_", *in)) {
      if (out) {
        *out++ = *in++;
      }
    } else {
      break;
    }
  }
  
  if (out) {
    *out = 0;
  }

  assert(*in == ESC_EMOJI);
  return in + 1;
}

/**
 * @brief A shorthand `g_snprintf` into a statically allocated buffer. Several
 * buffers are maintained internally so that nested va()'s are safe within
 * reasonable limits. This function is not thread safe.
 */
char *va(const char *format, ...) {
  static char strings[8][MAX_STRING_CHARS];
  static int32_t index;

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
  q_snprintf(s, MAX_QPATH, "(%4.2f %4.2f %4.2f)", v.x, v.y, v.z);

  return s;
}

/**
 * @brief Returns the value for the given key, or an empty string.
 */
char *InfoString_Get(const char *s, const char *key) {
  char pkey[512];
  static char value[2][512]; // use two buffers so compares work without stomping on each other
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

    if (!q_strcmp(key, pkey)) {
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
 * @brief Iterates the key-value pairs in an info string.
 */
const char *InfoString_Next(const char *s, char *key, char *value) {

  assert(s);

  while (*s == '\\') {
    s++;
  }

  if (!*s) {
    return NULL;
  }

  const char *src;

  src = s;
  while (*s && *s != '\\') {
    s++;
  }

  q_strlcpy(key, src, s - src + 1);

  if (*s == '\\') {
    s++;
  }

  src = s;
  while (*s && *s != '\\') {
    s++;
  }

  q_strlcpy(value, src, s - src + 1);

  if (*s == '\\') {
    s++;
  }

  while (*s == '\\') {
    s++;
  }

  return *s ? s : NULL;
}

/**
 * @brief Removes the key-value pair identified by `key` from the info string.
 * @return True if the key was found and removed, false otherwise.
 */
bool InfoString_Delete(char *s, const char *key) {
  char *start;
  char pkey[512];
  char value[512];
  char *o;

  if (q_strstr(key, "\\")) {
    return false;
  }

  while (true) {
    start = s;
    if (*s == '\\') {
      s++;
    }
    o = pkey;
    while (*s != '\\') {
      if (!*s) {
        return false;
      }
      *o++ = *s++;
    }
    *o = '\0';
    s++;

    o = value;
    while (*s != '\\' && *s) {
      if (!*s) {
        return false;
      }
      *o++ = *s++;
    }
    *o = '\0';

    if (!q_strcmp(key, pkey)) {
      memmove(start, s, q_strlen(s) + 1);
      return true;
    }

    if (!*s) {
      return false;
    }
  }
}

/**
 * @brief Returns true if the specified info string appears valid, false otherwise.
 */
bool InfoString_Validate(const char *s) {
  if (!s || !*s) {
    return false;
  }
  if (q_strstr(s, "\"")) {
    return false;
  }
  return true;
}

/**
 * @brief Inserts or updates the key-value pair in the info string.
 * @return True on success, false if the key or value contains invalid characters or exceeds length limits.
 */
bool InfoString_Set(char *s, const char *key, const char *value) {
  char newi[MAX_INFO_STRING_STRING * 16], *v;

  if (!q_strlen(key)) {
    return false;
  }

  if (q_strstr(key, "\\") || q_strstr(value, "\\")) {
    return false;
  }

  if (q_strstr(key, ";")) {
    return false;
  }

  if (q_strstr(key, "\"") || q_strstr(value, "\"")) {
    return false;
  }

  if (q_strlen(key) > MAX_INFO_STRING_KEY - 1 || q_strlen(value) > MAX_INFO_STRING_VALUE - 1) {
    return false;
  }

  InfoString_Delete(s, key);

  if (q_strlen(s)) {
    q_snprintf(newi, sizeof(newi), "\\%s\\%s", key, value ?: "");
  } else {
    q_snprintf(newi, sizeof(newi), "%s\\%s", key, value ?: "");
  }

  if (q_strlen(newi) + q_strlen(s) > MAX_INFO_STRING_STRING) {
    return false;
  }

  // only copy ascii values
  s += q_strlen(s);
  v = newi;
  while (*v) {
    char c = *v++;
    c &= 127; // strip high bits
    if (c >= 32 && c < 127) {
      *s++ = c;
    }
  }
  *s = '\0';

  return true;
}
