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
	unsigned short *player_num;

	// network messaging
	void (*ReadData)(void *buf, size_t len);
	int (*ReadChar)(void);
	int (*ReadByte)(void);
	int (*ReadShort)(void);
	int (*ReadLong)(void);
	char *(*ReadString)(void);
	void (*ReadPosition)(vec3_t pos);
	void (*ReadDir)(vec3_t dir);
	float (*ReadAngle)(void);

	// incoming server data stream
	size_buf_t *net_message;

	void (*Trace)(vec3_t start, vec3_t end, float radius, int mask);

	// context parameters
	r_pixel_t *width, *height;

	// view parameteres
	r_pixel_t *x, *y, *w, *h;

	// client time
	unsigned int *time;

	// 256 color palette for particle and effect colors
	unsigned *palette;

	r_image_t *(*LoadPic)(const char *name);
	void (*DrawPic)(r_pixel_t x, r_pixel_t y, float scale, const char *name);
	void (*DrawFill)(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, int c, float a);

	void (*BindFont)(const char *name, r_pixel_t *cw, r_pixel_t *ch);
	r_pixel_t (*StringWidth)(const char *s);
	size_t (*DrawString)(r_pixel_t x, r_pixel_t y, const char *s, int color);
} cg_import_t;

// exposed to the engine by the client game
typedef struct cg_export_s {
	int api_version;

	void (*Init)(void);
	void (*Shutdown)(void);

	void (*UpdateMedia)(void);

	boolean_t (*ParseMessage)(int cmd);

	float (*ThirdPerson)(player_state_t *ps);

	void (*DrawFrame)(player_state_t *ps);
} cg_export_t;

#endif /* __CGAME_H__ */
