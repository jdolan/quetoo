/*
 Copyright (C) 1999-2007 id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "common.h"

typedef enum {
	MON_PRINT = 1,
	MON_WARN,
	MON_ERROR
} mon_level_t;

void Mon_SendMessage(mon_level_t level, const char *msg);

void Mon_SendSelect_(const char *func, mon_level_t level, int32_t e, int32_t b, const char *msg);
void Mon_SendWinding_(const char *func, mon_level_t level, const vec3_t p[], int32_t n, const char *msg);
void Mon_SendPoint_(const char *func, mon_level_t level, const vec3_t p, const char *msg);

#define Mon_SendSelect(l, e, b, msg) Mon_SendSelect_(__func__, l, e, b, msg)
#define Mon_SendWinding(l, p, n, msg) Mon_SendWinding_(__func__, l, p, n, msg)
#define Mon_SendPoint(l, p, msg) Mon_SendPoint_(__func__, l, p, msg)

void Mon_Init(void);
_Bool Mon_Connect(const char *host);
void Mon_Shutdown(const char *msg);
