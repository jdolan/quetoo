/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __CGAME_H__
#define __CGAME_H__

#include "client/cl_types.h"

#define CGAME_API_VERSION 1

// exposed to the client game by the engine
typedef struct cg_import_s {

	void (*Print)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (*Debug)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (*Warn)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (*Error)(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

	cvar_t *(*Cvar)(const char *name, const char *value, int flags, const char *description);

	char *(*ConfigString)(int index);

	void (*Trace)(vec3_t start, vec3_t end, float radius, int masl);

	// view parameters
	int *width, *height;

	// client time
	int *time;

	// 256 color palette for particle and effect colors
	unsigned *palette;

	r_image_t *(*LoadPic)(const char *name);
	void (*DrawPic)(int x, int y, float scale, const char *name);
	void (*DrawFill)(int x, int y, int w, int h, int c, float a);

	void (*BindFont)(const char *name, int *cw, int *ch);
	int (*StringWidth)(const char *s);
	int (*DrawString)(int x, int y, const char *s, int color);
} cg_import_t;

// exposed to the engine by the client game
typedef struct cg_export_s {
	int api_version;

	void (*Init)(void);
	void (*Shutdown)(void);

	float (*ThirdPerson)(player_state_t *ps);

	void (*DrawHud)(player_state_t *ps);
} cg_export_t;

#endif /* __CGAME_H__ */
