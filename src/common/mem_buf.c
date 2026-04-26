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

#include "mem_buf.h"

/**
 * @brief Initializes a fixed-size memory buffer backed by the given data array.
 */
void Mem_InitBuffer(mem_buf_t *buf, byte *data, size_t len) {

  memset(buf, 0, sizeof(*buf));

  buf->data = data;
  buf->max_size = len;
}

/**
 * @brief Resets the buffer's write position to zero and clears the overflow flag.
 */
void Mem_ClearBuffer(mem_buf_t *buf) {

  buf->size = 0;
  buf->overflowed = false;
}

/**
 * @brief Advances the write cursor by `len` bytes and returns a pointer to the newly reserved region.
 * @details If `allow_overflow` is set and the buffer would overflow, the buffer is cleared first.
 *   Aborts if the buffer does not have `allow_overflow` set and an overflow would occur.
 */
void *Mem_AllocBuffer(mem_buf_t *buf, size_t len) {
  void *data;

  if (buf->size + len > buf->max_size) {
    const uint32_t delta = (uint32_t) (buf->size + len - buf->max_size);
    fprintf(stderr, "%s: Overflow by %u bytes\n", __func__, delta);

    if (!buf->allow_overflow) {
      fprintf(stderr, "Overflow without allow_overflow set\n");
      raise(SIGABRT);
    }

    if (len > buf->max_size) {
      fprintf(stderr, "%u is > buffer size %u\n", (uint32_t) len, (uint32_t) buf->max_size);
      raise(SIGABRT);
    }

    Mem_ClearBuffer(buf);
    buf->overflowed = true;
  }

  data = buf->data + buf->size;
  buf->size += len;

  return data;
}

/**
 * @brief Copies `len` bytes from `data` into the next available region of the buffer.
 */
void Mem_WriteBuffer(mem_buf_t *buf, const void *data, size_t len) {
  memcpy(Mem_AllocBuffer(buf, len), data, len);
}
