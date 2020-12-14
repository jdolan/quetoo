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
static struct {
	r_sprite_vertex_t vertexes[MAX_SPRITE_INSTANCES * 4];

	GLuint vertex_array;
	GLuint vertex_buffer;
	GLuint index_buffer;

	GHashTable *blend_depth_hash;

} r_sprites;

/**
 * @brief The draw program.
 */
static struct {
	GLuint name;

	GLuint uniforms;
	
	GLint in_position;
	GLint in_diffusemap;
	GLint in_next_diffusemap;
	GLint in_color;
	GLint in_lerp;

	GLint depth_range;
	GLint inv_viewport_size;
	GLint transition_size;
	
	GLint texture_diffusemap;
	GLint texture_lightgrid_fog;
	GLint texture_next_diffusemap;
	GLint texture_depth_stencil_attachment;

} r_sprite_program;

/**
 * @brief
 */
static void R_SpriteTextureCoordinates(const r_image_t *image, vec2_t *tl, vec2_t *tr, vec2_t *br, vec2_t *bl) {

	if (image->media.type == R_MEDIA_ATLAS_IMAGE) {
		r_atlas_image_t *atlas_image = (r_atlas_image_t *) image;

		*tl = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.y);
		*tr = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.y);
		*br = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.w);
		*bl = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.w);
	} else {

		*tl = Vec2(0, 0);
		*tr = Vec2(1, 0);
		*br = Vec2(1, 1);
		*bl = Vec2(0, 1);
	}
}

/**
 * @brief
 */
static const r_image_t *R_ResolveSpriteImage(const r_media_t *media, const float life) {

	const r_image_t *image;

	if (media->type == R_MEDIA_ANIMATION) {
		image = R_ResolveAnimation((r_animation_t *) media, life, 0);
	} else {
		image = (r_image_t *) media;
	}

	return image;
}

/**
 * @brief
 */
static _Bool R_CullSprite(const r_sprite_instance_t *s) {
	vec3_t mins, maxs;

	Cm_TraceBounds(s->vertexes[0].position, s->vertexes[2].position, Vec3_Zero(), Vec3_Zero(), &mins, &maxs);

	return R_CullBox(mins, maxs);
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test.
 */
void R_AddSprite(const r_sprite_t *s) {

	if (r_view.num_sprites == MAX_SPRITES) {
		Com_Debug(DEBUG_RENDERER, "MAX_SPRITES\n");
		return;
	}

	r_sprite_instance_t *out = &r_view.sprite_instances[r_view.num_sprite_instances];

	r_view.sprites[r_view.num_sprites] = *s;

	out->sprite = &r_view.sprites[r_view.num_sprites];
	out->beam = NULL;

	out->diffusemap = R_ResolveSpriteImage(s->media, s->life);

	const float aspect_ratio = (float) out->diffusemap->width / (float) out->diffusemap->height;
	const float half_size = s->size * .5f;

	vec3_t dir, right, up;
	
	if (Vec3_Equal(s->dir, Vec3_Zero())) {

		if (!s->axis || s->axis & (SPRITE_AXIS_X | SPRITE_AXIS_Y | SPRITE_AXIS_Z)) {
			dir = Vec3_Normalize(Vec3_Subtract(s->origin, r_view.origin));
		} else {
			dir = Vec3_Zero();

			for (int32_t i = 0; i < 3; i++) {
				if (s->axis & (1 << i)) {
					dir.xyz[i] = s->origin.xyz[i] - r_view.origin.xyz[i];
				}
			}

			dir = Vec3_Normalize(dir);
		}
	} else {
		dir = s->dir;
	}

	dir = Vec3_Euler(dir);
	dir.z = Degrees(s->rotation);

	Vec3_Vectors(dir, NULL, &right, &up);

	const vec3_t u = Vec3_Scale(up, half_size),
	             d = Vec3_Scale(up, -half_size),
	             l = Vec3_Scale(right, -half_size * aspect_ratio),
	             r = Vec3_Scale(right, half_size * aspect_ratio);
	
	out->vertexes[0].position = Vec3_Add(Vec3_Add(s->origin, u), l);
	out->vertexes[1].position = Vec3_Add(Vec3_Add(s->origin, u), r);
	out->vertexes[2].position = Vec3_Add(Vec3_Add(s->origin, d), r);
	out->vertexes[3].position = Vec3_Add(Vec3_Add(s->origin, d), l);

	if (R_CullSprite(out)) {
		return;
	}

	R_SpriteTextureCoordinates(out->diffusemap, &out->vertexes[0].diffusemap,
												&out->vertexes[1].diffusemap,
												&out->vertexes[2].diffusemap,
												&out->vertexes[3].diffusemap);

	out->next_diffusemap = NULL;
	out->lerp = 0.f;

	if (s->media->type == R_MEDIA_ANIMATION) {
		const r_animation_t *anim = (const r_animation_t *) s->media;

		out->next_diffusemap = R_ResolveAnimation(anim, s->life, 1);

		R_SpriteTextureCoordinates(out->next_diffusemap, &out->vertexes[0].next_diffusemap,
														 &out->vertexes[1].next_diffusemap,
														 &out->vertexes[2].next_diffusemap,
														 &out->vertexes[3].next_diffusemap);

		if (s->flags & SPRITE_LERP) {
			const float life_to_images = floorf(s->life * anim->num_frames);
			const float cur_frame = life_to_images / anim->num_frames;
			const float next_frame = (life_to_images + 1) / anim->num_frames;
			out->lerp = (s->life - cur_frame) / (next_frame - cur_frame);
		}
	}

	out->vertexes[0].color =
	out->vertexes[1].color =
	out->vertexes[2].color =
	out->vertexes[3].color = s->color;

	out->vertexes[0].lerp =
	out->vertexes[1].lerp =
	out->vertexes[2].lerp =
	out->vertexes[3].lerp = out->lerp;

	r_view.num_sprite_instances++;
	r_view.num_sprites++;
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test.
 */
void R_AddBeam(const r_beam_t *b) {

	if (r_view.num_beams == MAX_BEAMS) {
		Com_Debug(DEBUG_RENDERER, "MAX_BEAMS\n");
		return;
	}

	r_sprite_instance_t *in = &r_view.sprite_instances[r_view.num_sprite_instances];

	r_view.beams[r_view.num_beams] = *b;

	in->beam = &r_view.beams[r_view.num_beams];
	in->sprite = NULL;

	in->diffusemap = R_ResolveSpriteImage((r_media_t *) b->image, 0);
	in->next_diffusemap = NULL;

	in->lerp = 0.f;

	const float half_size = b->size * .5f;

	float length;
	const vec3_t up = Vec3_NormalizeLength(Vec3_Subtract(b->start, b->end), &length);
	length /= b->image->width * (b->size / b->image->height);

	const vec3_t right = Vec3_Scale(Vec3_Normalize(Vec3_Cross(up, Vec3_Subtract(r_view.origin, b->end))), half_size);

	in->vertexes[0].position = Vec3_Add(b->start, right);
	in->vertexes[1].position = Vec3_Add(b->end, right);
	in->vertexes[2].position = Vec3_Subtract(b->end, right);
	in->vertexes[3].position = Vec3_Subtract(b->start, right);

	if (R_CullSprite(in)) {
		return;
	}

	R_SpriteTextureCoordinates(in->diffusemap, &in->vertexes[0].diffusemap,
											   &in->vertexes[1].diffusemap,
											   &in->vertexes[2].diffusemap,
											   &in->vertexes[3].diffusemap);

	if (!(b->image->type & IT_MASK_CLAMP_EDGE)) {

		if (b->stretch) {
			length *= b->stretch;
		}

		in->vertexes[1].diffusemap.x *= length;
		in->vertexes[2].diffusemap.x = in->vertexes[1].diffusemap.x;

		if (b->translate) {
			for (int32_t i = 0; i < 4; i++) {
				in->vertexes[i].diffusemap.x += b->translate;
			}
		}
	}

	in->vertexes[0].color =
	in->vertexes[1].color =
	in->vertexes[2].color =
	in->vertexes[3].color = b->color;

	in->vertexes[0].lerp =
	in->vertexes[1].lerp =
	in->vertexes[2].lerp =
	in->vertexes[3].lerp = in->lerp;

	r_view.num_sprite_instances++;
	r_view.num_beams++;
}

/**
 * @brief
 */
void R_UpdateSprites(void) {

	glUseProgram(r_sprite_program.name);

	glUniform2f(r_sprite_program.depth_range, 1.0, MAX_WORLD_DIST);
	glUniform2f(r_sprite_program.inv_viewport_size, 1.0 / r_context.drawable_width, 1.0 / r_context.drawable_height);
	glUniform1f(r_sprite_program.transition_size, .0016f);

	R_GetError(NULL);

	r_sprite_instance_t *in = r_view.sprite_instances;
	r_sprite_vertex_t *out = r_sprites.vertexes;

	for (int32_t i = 0; i < r_view.num_sprite_instances; i++, in++, out += 4) {

		assert(in->sprite || in->beam);
		assert(in->diffusemap);

		if (in->sprite) {
			if (in->sprite->flags & SPRITE_NO_BLEND_DEPTH) {
				in->blend_depth = 0;
			} else {
				in->blend_depth = R_BlendDepthForPoint(in->sprite->origin, BLEND_DEPTH_SPRITE);
			}
		} else {
			in->blend_depth = R_BlendDepthForPoint(in->beam->start, BLEND_DEPTH_SPRITE);
		}

		in->blend_chain = NULL;

		r_sprite_instance_t *chain = g_hash_table_lookup(r_sprites.blend_depth_hash, GINT_TO_POINTER(in->blend_depth));
		if (chain == NULL) {
			g_hash_table_insert(r_sprites.blend_depth_hash, GINT_TO_POINTER(in->blend_depth), in);
		} else {
			while (chain->blend_chain) {
				chain = chain->blend_chain;
			}
			chain->blend_chain = in;
		}

		memcpy(out, in->vertexes, sizeof(in->vertexes));
		in->offset = i * 6 * sizeof(GLuint);
	}

	glBindBuffer(GL_ARRAY_BUFFER, r_sprites.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, r_view.num_sprite_instances * sizeof(in->vertexes), r_sprites.vertexes, GL_DYNAMIC_DRAW);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_DrawSprites(int32_t blend_depth) {

	if (!r_view.num_sprites) {
		return;
	}
	
	glDepthMask(GL_FALSE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	
	glEnable(GL_DEPTH_TEST);

	glUseProgram(r_sprite_program.name);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_uniforms.buffer);

	glBindVertexArray(r_sprites.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_sprites.vertex_buffer);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_sprites.index_buffer);
	
	glEnableVertexAttribArray(r_sprite_program.in_position);
	glEnableVertexAttribArray(r_sprite_program.in_diffusemap);
	glEnableVertexAttribArray(r_sprite_program.in_color);
	glVertexAttrib1f(r_sprite_program.in_lerp, 0.f);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTGRID_FOG);
	glBindTexture(GL_TEXTURE_3D, r_world_model->bsp->lightgrid->textures[3]->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DEPTH_STENCIL_ATTACHMENT);
	glBindTexture(GL_TEXTURE_2D, r_context.depth_stencil_attachment);

	r_sprite_instance_t *in = g_hash_table_lookup(r_sprites.blend_depth_hash, GINT_TO_POINTER(blend_depth));
	while (in) {

		if (in->next_diffusemap) {
			glActiveTexture(GL_TEXTURE0 + TEXTURE_NEXT_DIFFUSEMAP);
			glBindTexture(GL_TEXTURE_2D, in->next_diffusemap->texnum);

			glEnableVertexAttribArray(r_sprite_program.in_next_diffusemap);
			glEnableVertexAttribArray(r_sprite_program.in_lerp);
		} else {
			glDisableVertexAttribArray(r_sprite_program.in_next_diffusemap);
			glDisableVertexAttribArray(r_sprite_program.in_lerp);
		}

		glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);
		glBindTexture(GL_TEXTURE_2D, in->diffusemap->texnum);

		const ptrdiff_t stride = 6 * (ptrdiff_t) sizeof(GLuint);

		int32_t count = 0;
		for (r_sprite_instance_t *s = in; s; s = s->blend_chain) {
			if (s->offset == in->offset + count * stride &&
				s->diffusemap == in->diffusemap &&
				s->next_diffusemap == in->next_diffusemap) {
				count++;
			} else {
				break;
			}
		}

		glDrawElements(GL_TRIANGLES, count * 6, GL_UNSIGNED_INT, (GLvoid *) in->offset);
		r_view.count_sprite_draw_elements++;

		for (int32_t i = 0; i < count; i++) {
			in = in->blend_chain;
		}
	}

	g_hash_table_remove(r_sprites.blend_depth_hash, GINT_TO_POINTER(blend_depth));
	
	glActiveTexture(GL_TEXTURE0);
	
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
			R_ShaderDescriptor(GL_VERTEX_SHADER, "sprite_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "soften_fs.glsl", "sprite_fs.glsl", NULL),
			NULL);
	
	glUseProgram(r_sprite_program.name);

	r_sprite_program.uniforms = glGetUniformBlockIndex(r_sprite_program.name, "uniforms");
	glUniformBlockBinding(r_sprite_program.name, r_sprite_program.uniforms, 0);
	
	r_sprite_program.in_position = glGetAttribLocation(r_sprite_program.name, "in_position");
	r_sprite_program.in_diffusemap = glGetAttribLocation(r_sprite_program.name, "in_diffusemap");
	r_sprite_program.in_next_diffusemap = glGetAttribLocation(r_sprite_program.name, "in_next_diffusemap");
	r_sprite_program.in_color = glGetAttribLocation(r_sprite_program.name, "in_color");
	r_sprite_program.in_lerp = glGetAttribLocation(r_sprite_program.name, "in_lerp");

	r_sprite_program.depth_range = glGetUniformLocation(r_sprite_program.name, "depth_range");
	r_sprite_program.inv_viewport_size = glGetUniformLocation(r_sprite_program.name, "inv_viewport_size");
	r_sprite_program.transition_size = glGetUniformLocation(r_sprite_program.name, "transition_size");

	r_sprite_program.texture_diffusemap = glGetUniformLocation(r_sprite_program.name, "texture_diffusemap");
	r_sprite_program.texture_lightgrid_fog = glGetUniformLocation(r_sprite_program.name, "texture_lightgrid_fog");
	r_sprite_program.texture_next_diffusemap = glGetUniformLocation(r_sprite_program.name, "texture_next_diffusemap");
	r_sprite_program.texture_depth_stencil_attachment = glGetUniformLocation(r_sprite_program.name, "texture_depth_stencil_attachment");

	glUniform1i(r_sprite_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);
	glUniform1i(r_sprite_program.texture_lightgrid_fog, TEXTURE_LIGHTGRID_FOG);
	glUniform1i(r_sprite_program.texture_next_diffusemap, TEXTURE_NEXT_DIFFUSEMAP);
	glUniform1i(r_sprite_program.texture_depth_stencil_attachment, TEXTURE_DEPTH_STENCIL_ATTACHMENT);

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
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, next_diffusemap));
	glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, color));
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, lerp));

	glGenBuffers(1, &r_sprites.index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_sprites.index_buffer);

	GLuint indices[MAX_SPRITES * 6];
	for (GLuint i = 0, v = 0, e = 0; i < MAX_SPRITES; i++, v += 4, e += 6) {
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

	r_sprites.blend_depth_hash = g_hash_table_new(g_direct_hash, g_direct_equal);

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

	g_hash_table_destroy(r_sprites.blend_depth_hash);

	R_ShutdownSpriteProgram();
}
