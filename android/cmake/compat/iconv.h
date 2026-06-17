/*
 * iconv.h — minimal iconv replacement for the Android NDK (Bionic has no iconv).
 *
 * Placed on the include path ahead of any system header so <iconv.h> resolves
 * here. Covers the StringEncoding set Objectively requests (ASCII, ISO-8859-1,
 * UTF-16, UTF-32, UTF-8, WCHAR_T); Quetoo's in-game text only ever uses
 * UTF-8 <-> WCHAR_T. Exotic encodings (ISO-8859-2, MacRoman) are unsupported and
 * fail iconv_open. See android/DEPENDENCIES.md. Implemented in iconv.c.
 */
#ifndef QUETOO_COMPAT_ICONV_H
#define QUETOO_COMPAT_ICONV_H

#include <stddef.h>

typedef void *iconv_t;

iconv_t iconv_open(const char *tocode, const char *fromcode);
size_t  iconv(iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);
int     iconv_close(iconv_t cd);

#endif /* QUETOO_COMPAT_ICONV_H */
