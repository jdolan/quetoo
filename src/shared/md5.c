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

#include "md5.h"

#define MD5_S11 7
#define MD5_S12 12
#define MD5_S13 17
#define MD5_S14 22
#define MD5_S21 5
#define MD5_S22 9
#define MD5_S23 14
#define MD5_S24 20
#define MD5_S31 4
#define MD5_S32 11
#define MD5_S33 16
#define MD5_S34 23
#define MD5_S41 6
#define MD5_S42 10
#define MD5_S43 15
#define MD5_S44 21

static const uint32_t MD5_K[64] = {
  0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
  0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
  0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
  0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
  0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
  0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
  0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
  0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

#define ROTLEFT(a, b) ((a << b) | (a >> (32 - b)))
#define F(x, y, z) ((x & y) | (~x & z))
#define G(x, y, z) ((x & z) | (y & ~z))
#define H(x, y, z) (x ^ y ^ z)
#define I(x, y, z) (y ^ (x | ~z))

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    for (int i = 0, j = 0; i < 16; ++i, j += 4)
        x[i] = ((uint32_t)block[j]) | (((uint32_t)block[j+1]) << 8) | (((uint32_t)block[j+2]) << 16) | (((uint32_t)block[j+3]) << 24);

    // Round 1
    a = b + ROTLEFT((a + F(b,c,d) + x[0] + MD5_K[0]), MD5_S11);
    d = a + ROTLEFT((d + F(a,b,c) + x[1] + MD5_K[1]), MD5_S12);
    c = d + ROTLEFT((c + F(d,a,b) + x[2] + MD5_K[2]), MD5_S13);
    b = c + ROTLEFT((b + F(c,d,a) + x[3] + MD5_K[3]), MD5_S14);
    a = b + ROTLEFT((a + F(b,c,d) + x[4] + MD5_K[4]), MD5_S11);
    d = a + ROTLEFT((d + F(a,b,c) + x[5] + MD5_K[5]), MD5_S12);
    c = d + ROTLEFT((c + F(d,a,b) + x[6] + MD5_K[6]), MD5_S13);
    b = c + ROTLEFT((b + F(c,d,a) + x[7] + MD5_K[7]), MD5_S14);
    a = b + ROTLEFT((a + F(b,c,d) + x[8] + MD5_K[8]), MD5_S11);
    d = a + ROTLEFT((d + F(a,b,c) + x[9] + MD5_K[9]), MD5_S12);
    c = d + ROTLEFT((c + F(d,a,b) + x[10] + MD5_K[10]), MD5_S13);
    b = c + ROTLEFT((b + F(c,d,a) + x[11] + MD5_K[11]), MD5_S14);
    a = b + ROTLEFT((a + F(b,c,d) + x[12] + MD5_K[12]), MD5_S11);
    d = a + ROTLEFT((d + F(a,b,c) + x[13] + MD5_K[13]), MD5_S12);
    c = d + ROTLEFT((c + F(d,a,b) + x[14] + MD5_K[14]), MD5_S13);
    b = c + ROTLEFT((b + F(c,d,a) + x[15] + MD5_K[15]), MD5_S14);

    // Round 2
    a = b + ROTLEFT((a + G(b,c,d) + x[1] + MD5_K[16]), MD5_S21);
    d = a + ROTLEFT((d + G(a,b,c) + x[6] + MD5_K[17]), MD5_S22);
    c = d + ROTLEFT((c + G(d,a,b) + x[11] + MD5_K[18]), MD5_S23);
    b = c + ROTLEFT((b + G(c,d,a) + x[0] + MD5_K[19]), MD5_S24);
    a = b + ROTLEFT((a + G(b,c,d) + x[5] + MD5_K[20]), MD5_S21);
    d = a + ROTLEFT((d + G(a,b,c) + x[10] + MD5_K[21]), MD5_S22);
    c = d + ROTLEFT((c + G(d,a,b) + x[15] + MD5_K[22]), MD5_S23);
    b = c + ROTLEFT((b + G(c,d,a) + x[4] + MD5_K[23]), MD5_S24);
    a = b + ROTLEFT((a + G(b,c,d) + x[9] + MD5_K[24]), MD5_S21);
    d = a + ROTLEFT((d + G(a,b,c) + x[14] + MD5_K[25]), MD5_S22);
    c = d + ROTLEFT((c + G(d,a,b) + x[3] + MD5_K[26]), MD5_S23);
    b = c + ROTLEFT((b + G(c,d,a) + x[8] + MD5_K[27]), MD5_S24);
    a = b + ROTLEFT((a + G(b,c,d) + x[13] + MD5_K[28]), MD5_S21);
    d = a + ROTLEFT((d + G(a,b,c) + x[2] + MD5_K[29]), MD5_S22);
    c = d + ROTLEFT((c + G(d,a,b) + x[7] + MD5_K[30]), MD5_S23);
    b = c + ROTLEFT((b + G(c,d,a) + x[12] + MD5_K[31]), MD5_S24);

    // Round 3
    a = b + ROTLEFT((a + H(b,c,d) + x[5] + MD5_K[32]), MD5_S31);
    d = a + ROTLEFT((d + H(a,b,c) + x[8] + MD5_K[33]), MD5_S32);
    c = d + ROTLEFT((c + H(d,a,b) + x[11] + MD5_K[34]), MD5_S33);
    b = c + ROTLEFT((b + H(c,d,a) + x[14] + MD5_K[35]), MD5_S34);
    a = b + ROTLEFT((a + H(b,c,d) + x[1] + MD5_K[36]), MD5_S31);
    d = a + ROTLEFT((d + H(a,b,c) + x[4] + MD5_K[37]), MD5_S32);
    c = d + ROTLEFT((c + H(d,a,b) + x[7] + MD5_K[38]), MD5_S33);
    b = c + ROTLEFT((b + H(c,d,a) + x[10] + MD5_K[39]), MD5_S34);
    a = b + ROTLEFT((a + H(b,c,d) + x[13] + MD5_K[40]), MD5_S31);
    d = a + ROTLEFT((d + H(a,b,c) + x[0] + MD5_K[41]), MD5_S32);
    c = d + ROTLEFT((c + H(d,a,b) + x[3] + MD5_K[42]), MD5_S33);
    b = c + ROTLEFT((b + H(c,d,a) + x[6] + MD5_K[43]), MD5_S34);
    a = b + ROTLEFT((a + H(b,c,d) + x[9] + MD5_K[44]), MD5_S31);
    d = a + ROTLEFT((d + H(a,b,c) + x[12] + MD5_K[45]), MD5_S32);
    c = d + ROTLEFT((c + H(d,a,b) + x[15] + MD5_K[46]), MD5_S33);
    b = c + ROTLEFT((b + H(c,d,a) + x[2] + MD5_K[47]), MD5_S34);

    // Round 4
    a = b + ROTLEFT((a + I(b,c,d) + x[0] + MD5_K[48]), MD5_S41);
    d = a + ROTLEFT((d + I(a,b,c) + x[7] + MD5_K[49]), MD5_S42);
    c = d + ROTLEFT((c + I(d,a,b) + x[14] + MD5_K[50]), MD5_S43);
    b = c + ROTLEFT((b + I(c,d,a) + x[5] + MD5_K[51]), MD5_S44);
    a = b + ROTLEFT((a + I(b,c,d) + x[12] + MD5_K[52]), MD5_S41);
    d = a + ROTLEFT((d + I(a,b,c) + x[3] + MD5_K[53]), MD5_S42);
    c = d + ROTLEFT((c + I(d,a,b) + x[10] + MD5_K[54]), MD5_S43);
    b = c + ROTLEFT((b + I(c,d,a) + x[1] + MD5_K[55]), MD5_S44);
    a = b + ROTLEFT((a + I(b,c,d) + x[8] + MD5_K[56]), MD5_S41);
    d = a + ROTLEFT((d + I(a,b,c) + x[15] + MD5_K[57]), MD5_S42);
    c = d + ROTLEFT((c + I(d,a,b) + x[6] + MD5_K[58]), MD5_S43);
    b = c + ROTLEFT((b + I(c,d,a) + x[13] + MD5_K[59]), MD5_S44);
    a = b + ROTLEFT((a + I(b,c,d) + x[4] + MD5_K[60]), MD5_S41);
    d = a + ROTLEFT((d + I(a,b,c) + x[11] + MD5_K[61]), MD5_S42);
    c = d + ROTLEFT((c + I(d,a,b) + x[2] + MD5_K[62]), MD5_S43);
    b = c + ROTLEFT((b + I(c,d,a) + x[9] + MD5_K[63]), MD5_S44);

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

void md5_init(md5_ctx *ctx) {
    ctx->size = 0;
    ctx->buffer[0] = 0x67452301;
    ctx->buffer[1] = 0xefcdab89;
    ctx->buffer[2] = 0x98badcfe;
    ctx->buffer[3] = 0x10325476;
}

void md5_update(md5_ctx *ctx, const void *data, size_t size) {
    const uint8_t *ptr = (const uint8_t *)data;
    size_t index = ctx->size % 64;
    ctx->size += size;

    while (size--) {
        ctx->input[index++] = *ptr++;
        if (index == 64) {
            md5_transform(ctx->buffer, ctx->input);
            index = 0;
        }
    }
}

void md5_finalize(md5_ctx *ctx, uint8_t result[16]) {
    uint64_t total_bits = ctx->size * 8;  // save before padding modifies size

    size_t index = ctx->size % 64;
    size_t pad_size = (index < 56) ? (56 - index) : (120 - index);

    static const uint8_t padding[64] = {0x80};
    md5_update(ctx, padding, pad_size);

    uint8_t size_bytes[8];
    for (int i = 0; i < 8; i++) {
        size_bytes[i] = (uint8_t)(total_bits >> (i * 8));
    }
    md5_update(ctx, size_bytes, 8);

    for (int i = 0; i < 4; i++) {
        result[i*4]   = (uint8_t)(ctx->buffer[i]);
        result[i*4+1] = (uint8_t)(ctx->buffer[i] >> 8);
        result[i*4+2] = (uint8_t)(ctx->buffer[i] >> 16);
        result[i*4+3] = (uint8_t)(ctx->buffer[i] >> 24);
    }
}
