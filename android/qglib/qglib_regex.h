/*
 * qglib_regex.h — GRegex + GError for the qglib compatibility layer.
 *
 * Quetoo's entire regex surface is one pattern, `\$([a-z0-9_]+)`, used by
 * src/common/cvar.c to expand `$cvar` references via g_regex_replace_eval and a
 * callback that appends each substitution to a GString. This layer implements
 * just that surface on top of POSIX <regex.h> (present in glibc and Android's
 * Bionic) — not a from-scratch engine, not PCRE2.
 *
 * Source-compatible with the glib subset the engine touches: callers read
 * GError->message, fetch capture group 1, and reassign the returned string.
 */
#ifndef QGLIB_REGEX_H
#define QGLIB_REGEX_H

#include "qglib_string.h" /* GString — replace_eval's callback appends to one */

/* Forward declaration so this header is robust to include order: when entered
 * via a sibling sub-header the full GString definition may not yet be visible.
 * C11 permits this to coexist with the full typedef in qglib_string.h. */
typedef struct _GString GString;

/* GError is defined in qglib.h core (shared with qglib_io). */

/* Compile flags: values match glib's GRegexCompileFlags so call sites that OR
 * the glib constants together compile and behave identically. */
typedef enum {
	G_REGEX_CASELESS  = 1 << 0,
	G_REGEX_MULTILINE = 1 << 3,
	G_REGEX_DOTALL    = 1 << 4
} GRegexCompileFlags;

typedef struct _GRegex GRegex;
typedef struct _GMatchInfo GMatchInfo;

/* Replacement callback: append the substitution for this match to `result`.
 * Return TRUE to stop further replacement (glib semantics), FALSE to continue. */
typedef gboolean (*GRegexEvalCallback)(const GMatchInfo *match_info, GString *result, gpointer user_data);

GRegex *g_regex_new(const gchar *pattern, gint compile_options, gint match_options, GError **error);
gchar  *g_regex_replace_eval(GRegex *regex, const gchar *string, gssize string_len, gint start_position,
                             gint match_options, GRegexEvalCallback eval, gpointer user_data, GError **error);
gchar  *g_match_info_fetch(const GMatchInfo *match_info, gint match_num);

#endif /* QGLIB_REGEX_H */
