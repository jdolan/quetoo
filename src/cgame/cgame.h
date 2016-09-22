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

#ifndef __CGAME_H__
#define __CGAME_H__

#include "client/cl_types.h"

#define CGAME_API_VERSION 1

// exposed to the client game by the engine
typedef struct cg_import_s {

	void (*Print)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (*Debug_)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void (*Warn_)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void (*Error_)(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

	// milliseconds since launch
	uint32_t (*Time)(void);

	// zone memory management
	void *(*Malloc)(size_t size, mem_tag_t tag);
	void *(*LinkMalloc)(size_t size, void *parent);
	void (*Free)(void *p);
	void (*FreeTag)(mem_tag_t tag);

	// filesystem interaction
	int64_t (*LoadFile)(const char *path, void **buffer);
	void (*FreeFile)(void *buffer);

	// console variable and command interaction
	cvar_t *(*Cvar)(const char *name, const char *value, uint32_t flags, const char *desc);
	cmd_t *(*Cmd)(const char *name, CmdExecuteFunc Execute, uint32_t flags, const char *desc);

	char *(*ConfigString)(uint16_t index);

	// network messaging
	void (*ReadData)(void *buf, size_t len);
	int32_t (*ReadChar)(void);
	int32_t (*ReadByte)(void);
	int32_t (*ReadShort)(void);
	int32_t (*ReadLong)(void);
	char *(*ReadString)(void);
	vec_t (*ReadVector)(void);
	void (*ReadPosition)(vec3_t pos);
	void (*ReadDir)(vec3_t dir);
	vec_t (*ReadAngle)(void);
	void (*ReadAngles)(vec3_t angles);

	// public client structure
	cl_client_t *client;

	// entity string
	const char *(*EntityString)(void);

	// collision
	int32_t (*PointContents)(const vec3_t point);
	cm_trace_t (*Trace)(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
			const uint16_t skip, const int32_t contents);

	// PVS and PHS
	const r_bsp_leaf_t * (*LeafForPoint)(const vec3_t p, const r_bsp_model_t *model);
	_Bool (*LeafHearable)(const r_bsp_leaf_t *leaf);
	_Bool (*LeafVisible)(const r_bsp_leaf_t *leaf);

	// sound
	s_sample_t *(*LoadSample)(const char *name);
	void (*AddSample)(const s_play_sample_t *play);

	// OpenGL context
	r_context_t *context;

	// public renderer structure
	r_view_t *view;

	// 256 color palette for particle and effect colors
	img_palette_t *palette;
	void (*ColorFromPalette)(uint8_t c, vec_t *res);

	// RGB color management
	void (*Color)(const vec4_t color);

	// images and models
	r_image_t *(*LoadImage)(const char *name, r_image_type_t type);
	r_material_t *(*LoadMaterial)(const char *diffuse);
	r_model_t *(*LoadModel)(const char *name);
	r_model_t *(*WorldModel)(void);

	// scene building facilities
	void (*AddCorona)(const r_corona_t *c);
	const r_entity_t *(*AddEntity)(const r_entity_t *e);
	const r_entity_t
	*(*AddLinkedEntity)(const r_entity_t *parent, const r_model_t *model, const char *tag_name);
	void (*AddLight)(const r_light_t *l);
	void (*AddParticle)(const r_particle_t *p);
	void (*AddSustainedLight)(const r_sustained_light_t *s);

	// 2D drawing facilities
	void (*DrawImage)(r_pixel_t x, r_pixel_t y, vec_t scale, const r_image_t *image);
	void (*DrawFill)(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, int32_t c, vec_t a);

	void (*BindFont)(const char *name, r_pixel_t *cw, r_pixel_t *ch);
	r_pixel_t (*StringWidth)(const char *s);
	size_t (*DrawString)(r_pixel_t x, r_pixel_t y, const char *s, int32_t color);

	// Command Buffer
	void (*Cbuf)(const char *s);
} cg_import_t;

// exposed to the engine by the client game
typedef struct cg_export_s {
	uint16_t api_version;
	uint16_t protocol;

	void (*Init)(void);
	void (*Shutdown)(void);

	void (*ClearState)(void);
	void (*UpdateMedia)(void);
	void (*UpdateConfigString)(uint16_t index);
	void (*LoadClient)(cl_client_info_t *cl, const char *s);

	_Bool (*ParseMessage)(int32_t cmd);
	void (*PredictMovement)(const GList *cmds);
	void (*UpdateView)(const cl_frame_t *frame);
	void (*PopulateView)(const cl_frame_t *frame);
	void (*DrawFrame)(const cl_frame_t *frame);
} cg_export_t;

#endif /* __CGAME_H__ */
