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

#include "r_local.h"

/**
 * @brief
 */
typedef struct {
	vec3_t position;
	vec2_t diffusemap;
	color32_t color;
} r_sprite_vertex_t;

/**
 * @brief
 */
typedef struct {
	r_sprite_vertex_t sprites[MAX_SPRITES * 4];

	GLuint vertex_array;
	GLuint vertex_buffer;
	GLuint index_buffer;
} r_sprites_t;

static r_sprites_t r_sprites;

/**
 * @brief The draw program.
 */
static struct {
	GLuint name;
	
	GLint in_position;
	GLint in_diffusemap;
	GLint in_color;

	GLint projection;
	GLint view;

	GLint soft_particles;
	GLint camera_range;
	GLint inv_viewport_size;
	GLint transition_size;
	
	GLint texture_diffusemap;
	GLint depth_attachment;

	GLint brightness;
	GLint contrast;
	GLint saturation;
	GLint gamma;

	GLint fog_parameters;
	GLint fog_color;
} r_sprite_program;

static void R_AddSpriteInternal(const color_t color, const r_image_t *image, r_sprite_vertex_t *out) {
	_Bool is_current_batch = false;

	// FIXME: expand into a function???
	if (image->media.type == MEDIA_ATLAS_IMAGE) {
		r_atlas_image_t *atlas_image = (r_atlas_image_t *) image;

		is_current_batch = r_view.num_sprite_images && atlas_image->image.texnum == r_view.sprite_images[r_view.num_sprite_images - 1]->texnum;

		out[0].diffusemap = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.y);
		out[1].diffusemap = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.y);
		out[2].diffusemap = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.w);
		out[3].diffusemap = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.w);
	} else {

		is_current_batch = r_view.num_sprite_images && image == r_view.sprite_images[r_view.num_sprite_images - 1];

		out[0].diffusemap = Vec2(0, 0);
		out[1].diffusemap = Vec2(1, 0);
		out[2].diffusemap = Vec2(1, 1);
		out[3].diffusemap = Vec2(0, 1);
	}
	
	out[0].color = Color_Color32(color);
	out[1].color = out[0].color;
	out[2].color = out[0].color;
	out[3].color = out[0].color;

	if (is_current_batch) {
		r_view.sprite_batches[r_view.num_sprite_images - 1]++;
	} else {
		r_view.sprite_images[r_view.num_sprite_images] = image;
		r_view.sprite_batches[r_view.num_sprite_images] = 1;
		r_view.num_sprite_images++;
	}

	r_view.num_sprites++;
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test.
 */
void R_AddSprite(const r_sprite_t *p) {

	if (r_view.num_sprites == MAX_SPRITES) {
		return;
	}

	r_sprite_vertex_t *out = r_sprites.sprites + (r_view.num_sprites * 4);
	const float size = p->size * .5f;
	vec3_t dir = Vec3_Normalize(Vec3_Subtract(p->origin, r_view.origin)), right, up;
	dir = Vec3_Euler(dir);
	dir.z = Degrees(p->rotation);

	Vec3_Vectors(dir, NULL, &right, &up);

	const vec3_t u = Vec3_Scale(up, size), d = Vec3_Scale(up, -size), l = Vec3_Scale(right, -size), r = Vec3_Scale(right, size);
	
	out[0].position = Vec3_Add(Vec3_Add(p->origin, u), l);
	out[1].position = Vec3_Add(Vec3_Add(p->origin, u), r);
	out[2].position = Vec3_Add(Vec3_Add(p->origin, d), r);
	out[3].position = Vec3_Add(Vec3_Add(p->origin, d), l);

	R_AddSpriteInternal(p->color, p->image, out);
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test.
 */
void R_AddBeam(const r_beam_t *p) {

	if (r_view.num_sprites == MAX_SPRITES) {
		return;
	}

	r_sprite_vertex_t *out = r_sprites.sprites + (r_view.num_sprites * 4);
	const float size = p->size * .5f;
	float length;
	const vec3_t up = Vec3_NormalizeLength(Vec3_Subtract(p->start, p->end), &length),
		right = Vec3_Scale(Vec3_Normalize(Vec3_Cross(up, Vec3_Subtract(r_view.origin, p->end))), size);

	out[0].position = Vec3_Add(p->start, right);
	out[1].position = Vec3_Add(p->end, right);
	out[2].position = Vec3_Subtract(p->end, right);
	out[3].position = Vec3_Subtract(p->start, right);

	R_AddSpriteInternal(p->color, p->image, out);
	
	if (!(p->image->type & IT_MASK_CLAMP_EDGE)) {
		length /= p->image->width * (p->size / p->image->height);

		if (p->stretch) {
			length *= p->stretch;
		}

		out[1].diffusemap.x *= length;
		out[2].diffusemap.x = out[1].diffusemap.x;

		if (p->translate) {
			for (int32_t i = 0; i < 4; i++) {
				out[i].diffusemap.x += p->translate;
			}
		}
	}
}

/**
 * @brief
 */
void R_DrawSprites(void) {

	glDepthMask(GL_FALSE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);

	glUseProgram(r_sprite_program.name);

	glUniformMatrix4fv(r_sprite_program.projection, 1, GL_FALSE, (GLfloat *) r_locals.projection3D.m);
	glUniformMatrix4fv(r_sprite_program.view, 1, GL_FALSE, (GLfloat *) r_locals.view.m);

	glUniform2f(r_sprite_program.camera_range, 1.0, MAX_WORLD_DIST);
	glUniform2f(r_sprite_program.inv_viewport_size, 1.0 / r_context.drawable_width, 1.0 / r_context.drawable_height);
	glUniform1f(r_sprite_program.transition_size, .0064f);
	glUniform1i(r_sprite_program.soft_particles, r_soft_particles->integer);

	glUniform1f(r_sprite_program.brightness, r_brightness->value);
	glUniform1f(r_sprite_program.contrast, r_contrast->value);
	glUniform1f(r_sprite_program.saturation, r_saturation->value);
	glUniform1f(r_sprite_program.gamma, r_gamma->value);

	glUniform3fv(r_sprite_program.fog_parameters, 1, r_locals.fog_parameters.xyz);
	glUniform3fv(r_sprite_program.fog_color, 1, r_view.fog_color.xyz);

	glBindVertexArray(r_sprites.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_sprites.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, r_view.num_sprites * sizeof(r_sprite_vertex_t) * 4, r_sprites.sprites, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_sprites.index_buffer);
	
	glEnableVertexAttribArray(r_sprite_program.in_position);
	glEnableVertexAttribArray(r_sprite_program.in_diffusemap);
	glEnableVertexAttribArray(r_sprite_program.in_color);
	
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, r_context.depth_attachment);

	glActiveTexture(GL_TEXTURE0);

	ptrdiff_t offset = 0;

	for (uint32_t i = 0; i < r_view.num_sprite_images; i++) {
		glBindTexture(GL_TEXTURE_2D, r_view.sprite_images[i]->texnum);
		glDrawElements(GL_TRIANGLES, r_view.sprite_batches[i] * 6, GL_UNSIGNED_SHORT, (GLvoid *) offset);
		offset += (r_view.sprite_batches[i] * 6) * sizeof(uint16_t);
	}
	
	glBindVertexArray(0);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glDisable(GL_DEPTH_TEST);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glDepthMask(GL_TRUE);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitSpriteProgram(void) {

	memset(&r_sprite_program, 0, sizeof(r_sprite_program));

	r_sprite_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "sprite_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "common.glsl",  "soft_edges.glsl", "sprite_fs.glsl"),
			NULL);

	glUseProgram(r_sprite_program.name);
	
	r_sprite_program.in_position = glGetAttribLocation(r_sprite_program.name, "in_position");
	r_sprite_program.in_diffusemap = glGetAttribLocation(r_sprite_program.name, "in_diffusemap");
	r_sprite_program.in_color = glGetAttribLocation(r_sprite_program.name, "in_color");

	r_sprite_program.projection = glGetUniformLocation(r_sprite_program.name, "projection");
	r_sprite_program.view = glGetUniformLocation(r_sprite_program.name, "view");

	r_sprite_program.camera_range = glGetUniformLocation(r_sprite_program.name, "camera_range");
	r_sprite_program.inv_viewport_size = glGetUniformLocation(r_sprite_program.name, "inv_viewport_size");
	r_sprite_program.transition_size = glGetUniformLocation(r_sprite_program.name, "transition_size");
	r_sprite_program.soft_particles = glGetUniformLocation(r_sprite_program.name, "soft_particles");

	r_sprite_program.texture_diffusemap = glGetUniformLocation(r_sprite_program.name, "texture_diffusemap");
	r_sprite_program.depth_attachment = glGetUniformLocation(r_sprite_program.name, "depth_attachment");

	r_sprite_program.brightness = glGetUniformLocation(r_sprite_program.name, "brightness");
	r_sprite_program.contrast = glGetUniformLocation(r_sprite_program.name, "contrast");
	r_sprite_program.saturation = glGetUniformLocation(r_sprite_program.name, "saturation");
	r_sprite_program.gamma = glGetUniformLocation(r_sprite_program.name, "gamma");

	r_sprite_program.fog_parameters = glGetUniformLocation(r_sprite_program.name, "fog_parameters");
	r_sprite_program.fog_color = glGetUniformLocation(r_sprite_program.name, "fog_color");
	
	glUniform1i(r_sprite_program.texture_diffusemap, 0);
	glUniform1i(r_sprite_program.depth_attachment, 1);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitSprites(void) {

	memset(&r_sprites, 0, sizeof(r_sprites));

	glGenVertexArrays(1, &r_sprites.vertex_array);
	glBindVertexArray(r_sprites.vertex_array);

	glGenBuffers(1, &r_sprites.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_sprites.vertex_buffer);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, diffusemap));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, color));

	glGenBuffers(1, &r_sprites.index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_sprites.index_buffer);

	uint16_t indices[MAX_SPRITES * 6];

	for (uint32_t i = 0, v = 0, e = 0; i < MAX_SPRITES; i++, v += 4, e += 6) {
		indices[e + 0] = v + 0;
		indices[e + 1] = v + 1;
		indices[e + 2] = v + 2;
		indices[e + 3] = v + 0;
		indices[e + 4] = v + 2;
		indices[e + 5] = v + 3;
	}

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	R_GetError(NULL);

	R_InitSpriteProgram();
}

/**
 * @brief
 */
static void R_ShutdownSpriteProgram(void) {

	glDeleteProgram(r_sprite_program.name);

	r_sprite_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownSprites(void) {

	glDeleteVertexArrays(1, &r_sprites.vertex_array);
	glDeleteBuffers(1, &r_sprites.vertex_buffer);
	glDeleteBuffers(1, &r_sprites.index_buffer);

	R_ShutdownSpriteProgram();
}
