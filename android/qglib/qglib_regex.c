/*
 * qglib_regex.c — GRegex + GError over POSIX <regex.h>.
 * See qglib_regex.h. POSIX-targeted (Android/Linux); the differential test
 * (test_regex.c) pins behaviour against real glib (PCRE) for Quetoo's pattern.
 */
#define _POSIX_C_SOURCE 200809L /* expose POSIX regex.h under -std=c11/gnu11 */

#include <regex.h>
#include <string.h>

#include "qglib_regex.h"

/* Number of capture slots we track per match: group 0 (whole match) plus a
 * handful of subgroups. Quetoo's pattern has a single group; this is ample. */
#define QG_REGEX_NMATCH 16

struct _GRegex {
	regex_t compiled;
};

/* A match against a specific subject. `subject` is the string regexec ran on,
 * so g_match_info_fetch can slice substrings straight out of it. */
struct _GMatchInfo {
	const gchar *subject;
	regmatch_t   groups[QG_REGEX_NMATCH];
};

/* Allocate a GError carrying a regerror() description for `rc` on `preg`. */
static void qg_regex_set_error(GError **error, gint rc, const regex_t *preg) {
	if (!error) {
		return;
	}
	GError *e = g_malloc(sizeof(GError));
	e->domain = 0;
	e->code = rc;
	const gsize n = regerror(rc, preg, NULL, 0);
	e->message = g_malloc(n ? n : 1);
	if (n) {
		regerror(rc, preg, e->message, n);
	} else {
		e->message[0] = '\0';
	}
	*error = e;
}

GRegex *g_regex_new(const gchar *pattern, gint compile_options, gint match_options, GError **error) {
	(void) match_options; /* glib accepts default match options at compile time; nothing to map */

	int cflags = REG_EXTENDED; /* glib's regexes are closest to POSIX ERE */
	if (compile_options & G_REGEX_CASELESS) {
		cflags |= REG_ICASE;
	}
	if (compile_options & G_REGEX_MULTILINE) {
		cflags |= REG_NEWLINE; /* ^/$ match at embedded newlines, as in multiline mode */
	}
	/* G_REGEX_DOTALL has no POSIX ERE equivalent (REG_NEWLINE only affects how
	 * '.'/anchors treat '\n', and there is no "dot matches newline" toggle).
	 * Quetoo's pattern contains no '.', so the flag is a no-op here — ignore it. */

	GRegex *regex = g_malloc(sizeof(GRegex));
	const int rc = regcomp(&regex->compiled, pattern, cflags);
	if (rc != 0) {
		qg_regex_set_error(error, rc, &regex->compiled);
		regfree(&regex->compiled);
		g_free(regex);
		return NULL;
	}
	return regex;
}

gchar *g_match_info_fetch(const GMatchInfo *match_info, gint match_num) {
	if (!match_info || match_num < 0 || match_num >= QG_REGEX_NMATCH) {
		return NULL;
	}
	const regmatch_t *m = &match_info->groups[match_num];
	if (m->rm_so < 0) { /* group did not participate; glib yields "" here */
		return g_strdup("");
	}
	const gsize len = (gsize) (m->rm_eo - m->rm_so);
	gchar *out = g_malloc(len + 1);
	memcpy(out, match_info->subject + m->rm_so, len);
	out[len] = '\0';
	return out;
}

gchar *g_regex_replace_eval(GRegex *regex, const gchar *string, gssize string_len, gint start_position,
                            gint match_options, GRegexEvalCallback eval, gpointer user_data, GError **error) {
	(void) match_options;
	if (error) {
		*error = NULL;
	}

	/* Quetoo's input is always NUL-terminated; string_len == -1 means strlen. */
	const gsize len = (string_len < 0) ? strlen(string) : (gsize) string_len;

	GString *result = g_string_new(NULL);

	gsize pos = (start_position > 0) ? (gsize) start_position : 0;
	if (pos > len) {
		pos = len;
	}

	int eflags = 0; /* first attempt: pos is the true start of the buffer */
	while (pos <= len) {
		GMatchInfo match_info;
		match_info.subject = string;

		const int rc = regexec(&regex->compiled, string + pos, QG_REGEX_NMATCH, match_info.groups, eflags);
		if (rc != 0) {
			break; /* REG_NOMATCH (or error): no more matches */
		}

		/* regexec reports offsets relative to (string + pos); shift to absolute
		 * so g_match_info_fetch can index into the original subject. */
		for (gsize i = 0; i < QG_REGEX_NMATCH; i++) {
			if (match_info.groups[i].rm_so >= 0) {
				match_info.groups[i].rm_so += (regoff_t) pos;
				match_info.groups[i].rm_eo += (regoff_t) pos;
			}
		}

		const gsize ms = (gsize) match_info.groups[0].rm_so;
		const gsize me = (gsize) match_info.groups[0].rm_eo;

		/* Literal text between the previous position and this match. */
		if (ms > pos) {
			gchar *lit = g_malloc(ms - pos + 1);
			memcpy(lit, string + pos, ms - pos);
			lit[ms - pos] = '\0';
			g_string_append(result, lit);
			g_free(lit);
		}

		const gboolean stop = eval(&match_info, result, user_data);
		if (stop) {
			/* glib stops substituting but still emits the remaining tail. */
			pos = me;
			break;
		}

		if (me > ms) {
			pos = me;
		} else {
			/* Zero-length match: copy one char and step forward to make
			 * progress, otherwise we'd spin forever on the same position. */
			if (pos < len) {
				g_string_append_c(result, string[pos]);
			}
			pos += 1;
		}

		eflags = REG_NOTBOL; /* past the buffer start now: ^ must not match here */
	}

	/* Trailing literal after the last match (or the whole string if no match). */
	if (pos < len) {
		gchar *tail = g_malloc(len - pos + 1);
		memcpy(tail, string + pos, len - pos);
		tail[len - pos] = '\0';
		g_string_append(result, tail);
		g_free(tail);
	}

	return g_string_free(result, FALSE);
}

/* g_error_free is implemented in qglib_util.c (shared with qglib_io). */
