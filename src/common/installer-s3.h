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

#include "net/net_http.h"

/**
 * @brief An object entry from the S3 bucket listing.
 */
typedef struct {
  char bucket[MAX_QPATH];
  char key[MAX_OS_PATH];
  char etag[MAX_QPATH];
  int64_t size;
} s3_object_t;

extern GList *S3_ListObjects(const char *bucket);
extern bool S3_GetObject(const s3_object_t *obj);
