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

#include "net_types.h"

/**
 * @brief The asynchronous HTTP callback type.
 * @param status The HTTP response code.
 * @param data The returned data.
 * @param length The returned data length in bytes.
 */
typedef void (*Net_HttpCallback)(int32_t status, void *data, size_t length);

/**
 * @brief Synchronously GET the specified URL string, writing data and len.
 * @param url_string The URL string to GET.
 * @param data The returned data, which should be Mem_Free'd when no longer needed.
 * @param length The returned data length in bytes.
 * @return The HTTP response code.
 */
int32_t Net_HttpGet(const char *url_string, void **data, size_t *length);

/**
 * @brief Asynchronously GET the specified URL string.
 * @param url_string The URL string to GET.
 * @param callback The completion callback.
 */
void Net_HttpGetAsync(const char *url_string, Net_HttpCallback callback);
