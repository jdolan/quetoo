/*
 * iconv.c — minimal iconv (see iconv.h). Converts via an intermediate Unicode
 * code point. Native byte order for UTF-16/UTF-32/WCHAR_T (fine on Android: both
 * ends are this lib, and Quetoo only uses UTF-8 <-> WCHAR_T).
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "iconv.h"

typedef enum { CODEC_ASCII, CODEC_LATIN1, CODEC_UTF8, CODEC_UTF16, CODEC_UTF32 } qg_codec;

typedef struct { qg_codec from, to; } qg_iconv;

static int qg_codec_for(const char *name, qg_codec *out) {
	if (!strcmp(name, "UTF-8")) { *out = CODEC_UTF8; return 0; }
	if (!strcmp(name, "WCHAR_T") || !strcmp(name, "UTF-32")) { *out = CODEC_UTF32; return 0; }
	if (!strcmp(name, "UTF-16")) { *out = CODEC_UTF16; return 0; }
	if (!strcmp(name, "ASCII")) { *out = CODEC_ASCII; return 0; }
	if (!strcmp(name, "ISO-8859-1")) { *out = CODEC_LATIN1; return 0; }
	return -1; /* ISO-8859-2, MacRoman, ... unsupported (unused by Quetoo) */
}

iconv_t iconv_open(const char *tocode, const char *fromcode) {
	qg_codec from, to;
	if (qg_codec_for(fromcode, &from) || qg_codec_for(tocode, &to)) {
		errno = EINVAL;
		return (iconv_t) -1;
	}
	qg_iconv *cd = malloc(sizeof(*cd));
	if (!cd) {
		errno = ENOMEM;
		return (iconv_t) -1;
	}
	cd->from = from;
	cd->to = to;
	return cd;
}

int iconv_close(iconv_t cd) {
	free(cd);
	return 0;
}

/* Decode one code point. Returns bytes consumed, 0 if more input is needed
 * (incomplete), or -1 on an invalid sequence. */
static int qg_decode(qg_codec c, const uint8_t *in, size_t left, uint32_t *cp) {
	switch (c) {
		case CODEC_ASCII:
			if (left < 1) { return 0; }
			if (in[0] > 0x7f) { return -1; }
			*cp = in[0];
			return 1;
		case CODEC_LATIN1:
			if (left < 1) { return 0; }
			*cp = in[0];
			return 1;
		case CODEC_UTF8: {
			if (left < 1) { return 0; }
			const uint8_t b0 = in[0];
			if (b0 < 0x80) { *cp = b0; return 1; }
			int n;
			uint32_t v;
			if ((b0 & 0xe0) == 0xc0) { n = 2; v = b0 & 0x1f; }
			else if ((b0 & 0xf0) == 0xe0) { n = 3; v = b0 & 0x0f; }
			else if ((b0 & 0xf8) == 0xf0) { n = 4; v = b0 & 0x07; }
			else { return -1; }
			if (left < (size_t) n) { return 0; }
			for (int i = 1; i < n; i++) {
				if ((in[i] & 0xc0) != 0x80) { return -1; }
				v = (v << 6) | (in[i] & 0x3f);
			}
			*cp = v;
			return n;
		}
		case CODEC_UTF16: {
			if (left < 2) { return 0; }
			uint16_t w0;
			memcpy(&w0, in, 2);
			if (w0 >= 0xd800 && w0 <= 0xdbff) {
				if (left < 4) { return 0; }
				uint16_t w1;
				memcpy(&w1, in + 2, 2);
				if (w1 < 0xdc00 || w1 > 0xdfff) { return -1; }
				*cp = 0x10000 + (((uint32_t) (w0 - 0xd800) << 10) | (w1 - 0xdc00));
				return 4;
			}
			*cp = w0;
			return 2;
		}
		case CODEC_UTF32:
			if (left < 4) { return 0; }
			memcpy(cp, in, 4);
			return 4;
	}
	return -1;
}

/* Encode one code point. Returns bytes written, or -1 if there is no room. */
static int qg_encode(qg_codec c, uint32_t cp, uint8_t *out, size_t room) {
	switch (c) {
		case CODEC_ASCII:
			if (cp > 0x7f) { errno = EILSEQ; return -2; }
			if (room < 1) { return -1; }
			out[0] = (uint8_t) cp;
			return 1;
		case CODEC_LATIN1:
			if (cp > 0xff) { errno = EILSEQ; return -2; }
			if (room < 1) { return -1; }
			out[0] = (uint8_t) cp;
			return 1;
		case CODEC_UTF8:
			if (cp < 0x80) {
				if (room < 1) { return -1; }
				out[0] = (uint8_t) cp;
				return 1;
			} else if (cp < 0x800) {
				if (room < 2) { return -1; }
				out[0] = 0xc0 | (cp >> 6);
				out[1] = 0x80 | (cp & 0x3f);
				return 2;
			} else if (cp < 0x10000) {
				if (room < 3) { return -1; }
				out[0] = 0xe0 | (cp >> 12);
				out[1] = 0x80 | ((cp >> 6) & 0x3f);
				out[2] = 0x80 | (cp & 0x3f);
				return 3;
			}
			if (room < 4) { return -1; }
			out[0] = 0xf0 | (cp >> 18);
			out[1] = 0x80 | ((cp >> 12) & 0x3f);
			out[2] = 0x80 | ((cp >> 6) & 0x3f);
			out[3] = 0x80 | (cp & 0x3f);
			return 4;
		case CODEC_UTF16:
			if (cp < 0x10000) {
				if (room < 2) { return -1; }
				uint16_t w = (uint16_t) cp;
				memcpy(out, &w, 2);
				return 2;
			} else {
				if (room < 4) { return -1; }
				const uint32_t v = cp - 0x10000;
				uint16_t w0 = (uint16_t) (0xd800 + (v >> 10));
				uint16_t w1 = (uint16_t) (0xdc00 + (v & 0x3ff));
				memcpy(out, &w0, 2);
				memcpy(out + 2, &w1, 2);
				return 4;
			}
		case CODEC_UTF32:
			if (room < 4) { return -1; }
			memcpy(out, &cp, 4);
			return 4;
	}
	return -1;
}

size_t iconv(iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) {
	qg_iconv *c = cd;
	if (inbuf == NULL || *inbuf == NULL) {
		return 0; /* reset to initial state — stateless here */
	}
	while (*inbytesleft > 0) {
		uint32_t cp;
		const int got = qg_decode(c->from, (const uint8_t *) *inbuf, *inbytesleft, &cp);
		if (got == 0) { errno = EINVAL; return (size_t) -1; }  /* incomplete input */
		if (got < 0) { errno = EILSEQ; return (size_t) -1; }   /* invalid sequence */
		const int put = qg_encode(c->to, cp, (uint8_t *) *outbuf, *outbytesleft);
		if (put == -1) { errno = E2BIG; return (size_t) -1; }  /* no output room */
		if (put == -2) { return (size_t) -1; }                 /* unencodable (errno set) */
		*inbuf += got;
		*inbytesleft -= (size_t) got;
		*outbuf += put;
		*outbytesleft -= (size_t) put;
	}
	return 0;
}
