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

#ifndef __MONITOR_H__
#define __MONITOR_H__

#include "common.h"

void Mon_SendSelect(const char *msg, uint16_t ent_num, uint16_t brush_num, err_t error);
void Mon_SendWinding(const char *msg, const vec3_t *p, uint16_t num_points, err_t error);
void Mon_SendPosition(const char *msg, const vec3_t p, err_t error);

void Mon_Init(const char *host);
void Mon_Shutdown();

#endif /* __MONITOR_H__ */
