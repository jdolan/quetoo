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

#pragma once

#include "quetoo.h"

/**
 * @brief A fixed-sized buffer, used for accumulating e.g. net messages.
 */
typedef struct {
  byte *data;
  size_t maxSize; // maximum size before overflow
  size_t size; // current size
  size_t read;
} MemBuf;

void Mem_InitBuffer(MemBuf *buf, byte *data, size_t len);
void Mem_ClearBuffer(MemBuf *buf);
void *Mem_AllocBuffer(MemBuf *buf, size_t len);
void Mem_WriteBuffer(MemBuf *buf, const void *data, size_t len);
