/*
 * qglib_checksum.c — GChecksum implementation (MD5 only).
 *
 * Self-contained MD5 in the public-domain RFC 1321 style, routed through
 * QG_MALLOC/QG_FREE. g_checksum_get_string finalizes idempotently and returns
 * the lowercase hex digest, owned by the GChecksum (matching glib). The
 * differential test (test_checksum.c) pins the hex output against real glib.
 *
 * Only G_CHECKSUM_MD5 is supported — that is all the engine uses
 * (src/collision/cm_manifest.c). g_checksum_new returns NULL for any other
 * type, as glib does for an unsupported/invalid checksum type.
 */
#include <string.h>

#include "qglib_checksum.h"

/* ---- MD5 (RFC 1321) ---- */

typedef struct {
	guint32 a, b, c, d;     /* running state (A,B,C,D) */
	guint64 count;          /* total message length in bytes */
	guint8  buffer[64];     /* partial 64-byte block */
} qg_md5_ctx;

#define QG_MD5_F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define QG_MD5_G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define QG_MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define QG_MD5_I(x, y, z) ((y) ^ ((x) | ~(z)))

#define QG_MD5_ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define QG_MD5_STEP(f, a, b, c, d, x, t, s) do { \
	(a) += f((b), (c), (d)) + (x) + (guint32) (t); \
	(a) = QG_MD5_ROTL((a), (s)); \
	(a) += (b); \
} while (0)

static void qg_md5_init(qg_md5_ctx *ctx) {
	ctx->a = 0x67452301;
	ctx->b = 0xefcdab89;
	ctx->c = 0x98badcfe;
	ctx->d = 0x10325476;
	ctx->count = 0;
}

/* Process one 64-byte block. Input is read little-endian, per RFC 1321. */
static void qg_md5_block(qg_md5_ctx *ctx, const guint8 *p) {
	guint32 x[16];
	for (int i = 0; i < 16; i++) {
		x[i] = (guint32) p[i * 4] |
		       ((guint32) p[i * 4 + 1] << 8) |
		       ((guint32) p[i * 4 + 2] << 16) |
		       ((guint32) p[i * 4 + 3] << 24);
	}

	guint32 a = ctx->a, b = ctx->b, c = ctx->c, d = ctx->d;

	/* Round 1 */
	QG_MD5_STEP(QG_MD5_F, a, b, c, d, x[0],  0xd76aa478, 7);
	QG_MD5_STEP(QG_MD5_F, d, a, b, c, x[1],  0xe8c7b756, 12);
	QG_MD5_STEP(QG_MD5_F, c, d, a, b, x[2],  0x242070db, 17);
	QG_MD5_STEP(QG_MD5_F, b, c, d, a, x[3],  0xc1bdceee, 22);
	QG_MD5_STEP(QG_MD5_F, a, b, c, d, x[4],  0xf57c0faf, 7);
	QG_MD5_STEP(QG_MD5_F, d, a, b, c, x[5],  0x4787c62a, 12);
	QG_MD5_STEP(QG_MD5_F, c, d, a, b, x[6],  0xa8304613, 17);
	QG_MD5_STEP(QG_MD5_F, b, c, d, a, x[7],  0xfd469501, 22);
	QG_MD5_STEP(QG_MD5_F, a, b, c, d, x[8],  0x698098d8, 7);
	QG_MD5_STEP(QG_MD5_F, d, a, b, c, x[9],  0x8b44f7af, 12);
	QG_MD5_STEP(QG_MD5_F, c, d, a, b, x[10], 0xffff5bb1, 17);
	QG_MD5_STEP(QG_MD5_F, b, c, d, a, x[11], 0x895cd7be, 22);
	QG_MD5_STEP(QG_MD5_F, a, b, c, d, x[12], 0x6b901122, 7);
	QG_MD5_STEP(QG_MD5_F, d, a, b, c, x[13], 0xfd987193, 12);
	QG_MD5_STEP(QG_MD5_F, c, d, a, b, x[14], 0xa679438e, 17);
	QG_MD5_STEP(QG_MD5_F, b, c, d, a, x[15], 0x49b40821, 22);

	/* Round 2 */
	QG_MD5_STEP(QG_MD5_G, a, b, c, d, x[1],  0xf61e2562, 5);
	QG_MD5_STEP(QG_MD5_G, d, a, b, c, x[6],  0xc040b340, 9);
	QG_MD5_STEP(QG_MD5_G, c, d, a, b, x[11], 0x265e5a51, 14);
	QG_MD5_STEP(QG_MD5_G, b, c, d, a, x[0],  0xe9b6c7aa, 20);
	QG_MD5_STEP(QG_MD5_G, a, b, c, d, x[5],  0xd62f105d, 5);
	QG_MD5_STEP(QG_MD5_G, d, a, b, c, x[10], 0x02441453, 9);
	QG_MD5_STEP(QG_MD5_G, c, d, a, b, x[15], 0xd8a1e681, 14);
	QG_MD5_STEP(QG_MD5_G, b, c, d, a, x[4],  0xe7d3fbc8, 20);
	QG_MD5_STEP(QG_MD5_G, a, b, c, d, x[9],  0x21e1cde6, 5);
	QG_MD5_STEP(QG_MD5_G, d, a, b, c, x[14], 0xc33707d6, 9);
	QG_MD5_STEP(QG_MD5_G, c, d, a, b, x[3],  0xf4d50d87, 14);
	QG_MD5_STEP(QG_MD5_G, b, c, d, a, x[8],  0x455a14ed, 20);
	QG_MD5_STEP(QG_MD5_G, a, b, c, d, x[13], 0xa9e3e905, 5);
	QG_MD5_STEP(QG_MD5_G, d, a, b, c, x[2],  0xfcefa3f8, 9);
	QG_MD5_STEP(QG_MD5_G, c, d, a, b, x[7],  0x676f02d9, 14);
	QG_MD5_STEP(QG_MD5_G, b, c, d, a, x[12], 0x8d2a4c8a, 20);

	/* Round 3 */
	QG_MD5_STEP(QG_MD5_H, a, b, c, d, x[5],  0xfffa3942, 4);
	QG_MD5_STEP(QG_MD5_H, d, a, b, c, x[8],  0x8771f681, 11);
	QG_MD5_STEP(QG_MD5_H, c, d, a, b, x[11], 0x6d9d6122, 16);
	QG_MD5_STEP(QG_MD5_H, b, c, d, a, x[14], 0xfde5380c, 23);
	QG_MD5_STEP(QG_MD5_H, a, b, c, d, x[1],  0xa4beea44, 4);
	QG_MD5_STEP(QG_MD5_H, d, a, b, c, x[4],  0x4bdecfa9, 11);
	QG_MD5_STEP(QG_MD5_H, c, d, a, b, x[7],  0xf6bb4b60, 16);
	QG_MD5_STEP(QG_MD5_H, b, c, d, a, x[10], 0xbebfbc70, 23);
	QG_MD5_STEP(QG_MD5_H, a, b, c, d, x[13], 0x289b7ec6, 4);
	QG_MD5_STEP(QG_MD5_H, d, a, b, c, x[0],  0xeaa127fa, 11);
	QG_MD5_STEP(QG_MD5_H, c, d, a, b, x[3],  0xd4ef3085, 16);
	QG_MD5_STEP(QG_MD5_H, b, c, d, a, x[6],  0x04881d05, 23);
	QG_MD5_STEP(QG_MD5_H, a, b, c, d, x[9],  0xd9d4d039, 4);
	QG_MD5_STEP(QG_MD5_H, d, a, b, c, x[12], 0xe6db99e5, 11);
	QG_MD5_STEP(QG_MD5_H, c, d, a, b, x[15], 0x1fa27cf8, 16);
	QG_MD5_STEP(QG_MD5_H, b, c, d, a, x[2],  0xc4ac5665, 23);

	/* Round 4 */
	QG_MD5_STEP(QG_MD5_I, a, b, c, d, x[0],  0xf4292244, 6);
	QG_MD5_STEP(QG_MD5_I, d, a, b, c, x[7],  0x432aff97, 10);
	QG_MD5_STEP(QG_MD5_I, c, d, a, b, x[14], 0xab9423a7, 15);
	QG_MD5_STEP(QG_MD5_I, b, c, d, a, x[5],  0xfc93a039, 21);
	QG_MD5_STEP(QG_MD5_I, a, b, c, d, x[12], 0x655b59c3, 6);
	QG_MD5_STEP(QG_MD5_I, d, a, b, c, x[3],  0x8f0ccc92, 10);
	QG_MD5_STEP(QG_MD5_I, c, d, a, b, x[10], 0xffeff47d, 15);
	QG_MD5_STEP(QG_MD5_I, b, c, d, a, x[1],  0x85845dd1, 21);
	QG_MD5_STEP(QG_MD5_I, a, b, c, d, x[8],  0x6fa87e4f, 6);
	QG_MD5_STEP(QG_MD5_I, d, a, b, c, x[15], 0xfe2ce6e0, 10);
	QG_MD5_STEP(QG_MD5_I, c, d, a, b, x[6],  0xa3014314, 15);
	QG_MD5_STEP(QG_MD5_I, b, c, d, a, x[13], 0x4e0811a1, 21);
	QG_MD5_STEP(QG_MD5_I, a, b, c, d, x[4],  0xf7537e82, 6);
	QG_MD5_STEP(QG_MD5_I, d, a, b, c, x[11], 0xbd3af235, 10);
	QG_MD5_STEP(QG_MD5_I, c, d, a, b, x[2],  0x2ad7d2bb, 15);
	QG_MD5_STEP(QG_MD5_I, b, c, d, a, x[9],  0xeb86d391, 21);

	ctx->a += a;
	ctx->b += b;
	ctx->c += c;
	ctx->d += d;
}

static void qg_md5_update(qg_md5_ctx *ctx, const guint8 *data, gsize len) {
	gsize used = (gsize) (ctx->count & 63);
	ctx->count += len;

	if (used) {
		const gsize avail = 64 - used;
		if (len < avail) {
			memcpy(ctx->buffer + used, data, len);
			return;
		}
		memcpy(ctx->buffer + used, data, avail);
		qg_md5_block(ctx, ctx->buffer);
		data += avail;
		len -= avail;
	}

	while (len >= 64) {
		qg_md5_block(ctx, data);
		data += 64;
		len -= 64;
	}

	if (len) {
		memcpy(ctx->buffer, data, len);
	}
}

/* Finalize into 16 raw digest bytes (little-endian state words). */
static void qg_md5_final(qg_md5_ctx *ctx, guint8 out[16]) {
	const guint64 bits = ctx->count << 3;
	gsize used = (gsize) (ctx->count & 63);

	static const guint8 pad[64] = { 0x80 };
	const gsize padlen = (used < 56) ? (56 - used) : (120 - used);
	qg_md5_update(ctx, pad, padlen);

	guint8 lenbytes[8];
	for (int i = 0; i < 8; i++) {
		lenbytes[i] = (guint8) (bits >> (i * 8));
	}
	qg_md5_update(ctx, lenbytes, 8);

	const guint32 words[4] = { ctx->a, ctx->b, ctx->c, ctx->d };
	for (int i = 0; i < 4; i++) {
		out[i * 4]     = (guint8) (words[i]);
		out[i * 4 + 1] = (guint8) (words[i] >> 8);
		out[i * 4 + 2] = (guint8) (words[i] >> 16);
		out[i * 4 + 3] = (guint8) (words[i] >> 24);
	}
}

/* ---- GChecksum ---- */

struct _GChecksum {
	qg_md5_ctx ctx;
	gboolean   finalized;
	gchar      str[33];     /* 32 lowercase hex digits + NUL */
};

GChecksum *g_checksum_new(GChecksumType checksum_type) {
	if (checksum_type != G_CHECKSUM_MD5) {
		return NULL;  /* only MD5 is implemented (all the engine needs) */
	}
	GChecksum *checksum = QG_MALLOC(sizeof(*checksum));
	if (!checksum) {
		return NULL;
	}
	qg_md5_init(&checksum->ctx);
	checksum->finalized = FALSE;
	checksum->str[0] = '\0';
	return checksum;
}

void g_checksum_update(GChecksum *checksum, const guchar *data, gssize length) {
	if (!checksum || checksum->finalized) {
		return;
	}
	const gsize len = (length < 0) ? strlen((const gchar *) data) : (gsize) length;
	qg_md5_update(&checksum->ctx, (const guint8 *) data, len);
}

const gchar *g_checksum_get_string(GChecksum *checksum) {
	static const gchar hex[] = "0123456789abcdef";
	if (!checksum) {
		return NULL;
	}
	if (!checksum->finalized) {
		guint8 digest[16];
		qg_md5_final(&checksum->ctx, digest);
		for (int i = 0; i < 16; i++) {
			checksum->str[i * 2]     = hex[digest[i] >> 4];
			checksum->str[i * 2 + 1] = hex[digest[i] & 0x0f];
		}
		checksum->str[32] = '\0';
		checksum->finalized = TRUE;
	}
	return checksum->str;
}

void g_checksum_free(GChecksum *checksum) {
	QG_FREE(checksum);
}
