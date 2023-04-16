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

static cvar_t *r_sprite_lerp;
static cvar_t *r_sprite_soften;

/**
 * @brief
 */
static struct {
	r_sprite_vertex_t vertexes[MAX_SPRITE_INSTANCES * 4];

	GLuint vertex_array;
	GLuint vertex_buffer;
	GLuint elements_buffer;

	GHashTable *blend_depth_hash;

	GLuint depth_attachment;

} r_sprites;

/**
 * @brief The draw program.
 */
static struct {
	GLuint name;
	
	GLuint uniforms_block;
	GLuint lights_block;
	
	GLint in_position;
	GLint in_diffusemap;
	GLint in_next_diffusemap;
	GLint in_color;
	GLint in_lerp;
	GLint in_softness;
	GLint in_lighting;
	
	GLint texture_diffusemap;
	GLint texture_lightgrid_ambient;
	GLint texture_lightgrid_diffuse;
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

		*tl = Vec2(0.f, 0.f);
		*tr = Vec2(1.f, 0.f);
		*br = Vec2(1.f, 1.f);
		*bl = Vec2(0.f, 1.f);
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
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test, and returns a pointer to the sprite
 * slot it fills.
 */
r_sprite_t *R_AddSprite(r_view_t *view, const r_sprite_t *s) {

	assert(s->media);

	if (view->num_sprites == MAX_SPRITES) {
		Com_Debug(DEBUG_RENDERER, "MAX_SPRITES\n");
		return NULL;
	}

	const float radius = (s->size ?: Maxf(s->width, s->height)) * .5f;

	if (R_CulludeSphere(view, s->origin, radius)) {
		return NULL;
	}

	view->sprites[view->num_sprites] = *s;
	view->num_sprites++;

	return &view->sprites[view->num_sprites - 1];
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test, and returns a pointer to the sprite
 * slot it fills.
 */
r_beam_t *R_AddBeam(r_view_t *view, const r_beam_t *b) {

	if (view->num_beams == MAX_BEAMS) {
		Com_Debug(DEBUG_RENDERER, "MAX_BEAMS\n");
		return NULL;
	}

	const box3_t bounds = Cm_TraceBounds(b->start, b->end, Box3f(b->size, b->size, b->size));

	if (R_CulludeBox(view, bounds)) {
		return NULL;
	}

	view->beams[view->num_beams] = *b;
	view->num_beams++;

	return &view->beams[view->num_beams - 1];
}

/**
 * @brief
 */
static r_sprite_instance_t *R_AllocSpriteInstance(r_view_t *view) {

	if (view->num_sprite_instances == MAX_SPRITE_INSTANCES) {
		Com_Debug(DEBUG_RENDERER, "MAX_SPRITE_INSTANCES\n");
		return NULL;
	}

	r_sprite_instance_t *in = &view->sprite_instances[view->num_sprite_instances];
	memset(in, 0, sizeof(*in));

	in->index = view->num_sprite_instances++;
	in->vertexes = r_sprites.vertexes + 4 * in->index;

	return in;
}

/**
 * @brief
 */
static void R_UpdateSprite(r_view_t *view, const r_sprite_t *s) {

	r_sprite_instance_t *in = R_AllocSpriteInstance(view);
	if (!in) {
		return;
	}

	in->flags = s->flags;
	in->diffusemap = R_ResolveSpriteImage(s->media, s->life);

	const float aspect_ratio = (float) in->diffusemap->width / (float) in->diffusemap->height;
	const float half_width = (s->size ?: s->width) * .5f;
	const float half_height = (s->size ?: s->height) * .5f;

	vec3_t dir, right, up;

	if (Vec3_Equal(s->dir, Vec3_Zero())) {

		if (s->axis == SPRITE_AXIS_ALL) {
			if (s->rotation) {
				dir = view->angles;
				dir.z = Degrees(s->rotation);
				Vec3_Vectors(dir, NULL, &right, &up);
			} else {
				right = view->right;
				up = view->up;
			}
		} else {
			dir = Vec3_Zero();

			for (int32_t i = 0; i < 3; i++) {
				if (s->axis & (1 << i)) {
					dir.xyz[i] = view->forward.xyz[i];
				}
			}

			dir = Vec3_Euler(Vec3_Normalize(dir));
			dir.z = Degrees(s->rotation);
			Vec3_Vectors(dir, NULL, &right, &up);
		}
	} else {
		dir = Vec3_Euler(s->dir);
		dir.z = Degrees(s->rotation);
		Vec3_Vectors(dir, NULL, &right, &up);
	}

	const vec3_t u = Vec3_Scale(up, half_height),
				 d = Vec3_Scale(up, -half_height),
				 l = Vec3_Scale(right, -half_width * aspect_ratio),
				 r = Vec3_Scale(right, half_width * aspect_ratio);

	in->vertexes[0].position = Vec3_Add(Vec3_Add(s->origin, u), l);
	in->vertexes[1].position = Vec3_Add(Vec3_Add(s->origin, u), r);
	in->vertexes[2].position = Vec3_Add(Vec3_Add(s->origin, d), r);
	in->vertexes[3].position = Vec3_Add(Vec3_Add(s->origin, d), l);

	R_SpriteTextureCoordinates(in->diffusemap, &in->vertexes[0].diffusemap,
											   &in->vertexes[1].diffusemap,
											   &in->vertexes[2].diffusemap,
											   &in->vertexes[3].diffusemap);

	float lerp = 0.f;

	if (s->media->type == R_MEDIA_ANIMATION) {
		const r_animation_t *anim = (const r_animation_t *) s->media;

		in->next_diffusemap = R_ResolveAnimation(anim, s->life, 1);

		R_SpriteTextureCoordinates(in->next_diffusemap, &in->vertexes[0].next_diffusemap,
														&in->vertexes[1].next_diffusemap,
														&in->vertexes[2].next_diffusemap,
														&in->vertexes[3].next_diffusemap);

		if (!(s->flags & SPRITE_NO_LERP) && r_sprite_lerp->integer) {
			const float life_to_images = floorf(s->life * anim->num_frames);
			const float cur_frame = life_to_images / anim->num_frames;
			const float next_frame = (life_to_images + 1) / anim->num_frames;
			lerp = (s->life - cur_frame) / (next_frame - cur_frame);
		}
	}

	in->vertexes[0].color =
	in->vertexes[1].color =
	in->vertexes[2].color =
	in->vertexes[3].color = s->color;

	in->vertexes[0].lerp =
	in->vertexes[1].lerp =
	in->vertexes[2].lerp =
	in->vertexes[3].lerp = lerp;

	if (s->flags & SPRITE_NO_BLEND_DEPTH) {
		in->blend_depth = INT32_MAX;
	} else {
		in->blend_depth = R_BlendDepthForPoint(view, s->origin, BLEND_DEPTH_SPRITE);
	}

	in->vertexes[0].softness =
	in->vertexes[1].softness =
	in->vertexes[2].softness =
	in->vertexes[3].softness = r_sprite_soften->value * s->softness;

	in->vertexes[0].lighting =
	in->vertexes[1].lighting =
	in->vertexes[2].lighting =
	in->vertexes[3].lighting = s->lighting;
}

/**
 * @brief Break the beam into segments based on blend depth transitions.
 */
void R_UpdateBeam(r_view_t *view, const r_beam_t *b) {
	float length;

	const vec3_t up = Vec3_NormalizeLength(Vec3_Subtract(b->start, b->end), &length);
	length /= b->image->width * (b->size / b->image->height);

	const float half_size = b->size * .5f;
	const vec3_t right = Vec3_Scale(Vec3_Normalize(Vec3_Cross(up, Vec3_Subtract(view->origin, b->end))), half_size);

	vec2_t texcoords[4];
	R_SpriteTextureCoordinates(b->image, &texcoords[0], &texcoords[1], &texcoords[2], &texcoords[3]);

	if (!(b->image->type & IT_MASK_CLAMP_EDGE)) {

		if (b->stretch) {
			length *= b->stretch;
		}

		texcoords[1].x *= length;
		texcoords[2].x = texcoords[1].x;

		if (b->translate) {
			for (int32_t i = 0; i < 4; i++) {
				texcoords[i].x += b->translate;
			}
		}
	}

	float step = 1.f;
	for (float frac = 0.f; frac < 1.f; ) {

		const vec3_t x = Vec3_Mix(b->start, b->end, frac);
		const vec3_t y = Vec3_Mix(b->start, b->end, frac + step);

		const int32_t x_depth = R_BlendDepthForPoint(view, x, BLEND_DEPTH_SPRITE);
		const int32_t y_depth = R_BlendDepthForPoint(view, y, BLEND_DEPTH_SPRITE);
		if (x_depth != y_depth) {
			if (step > .0625f) {
				step *= .5f;
				continue;
			}
		}

		vec3_t positions[4];
		positions[0] = Vec3_Add(x, right);
		positions[1] = Vec3_Add(y, right);
		positions[2] = Vec3_Subtract(y, right);
		positions[3] = Vec3_Subtract(x, right);

		r_sprite_instance_t *in = R_AllocSpriteInstance(view);
		if (!in) {
			return;
		}

		in->flags = b->flags;
		in->diffusemap = b->image;

		in->vertexes[0].position = positions[0];
		in->vertexes[1].position = positions[1];
		in->vertexes[2].position = positions[2];
		in->vertexes[3].position = positions[3];

		in->vertexes[0].diffusemap = texcoords[0];
		in->vertexes[1].diffusemap = texcoords[1];
		in->vertexes[2].diffusemap = texcoords[2];
		in->vertexes[3].diffusemap = texcoords[3];

		const float xs = (texcoords[0].x * (1.f - frac)) + (texcoords[1].x * frac);
		const float ys = (texcoords[0].x * (1.f - (frac + step))) + (texcoords[1].x * (frac + step));

		in->vertexes[0].diffusemap.x = xs;
		in->vertexes[1].diffusemap.x = ys;
		in->vertexes[2].diffusemap.x = ys;
		in->vertexes[3].diffusemap.x = xs;

		in->vertexes[0].color =
		in->vertexes[1].color =
		in->vertexes[2].color =
		in->vertexes[3].color = b->color;

		in->vertexes[0].lerp =
		in->vertexes[1].lerp =
		in->vertexes[2].lerp =
		in->vertexes[3].lerp = 0.f;

		in->vertexes[0].softness =
		in->vertexes[1].softness =
		in->vertexes[2].softness =
		in->vertexes[3].softness = r_sprite_soften->value * b->softness;

		in->vertexes[0].lighting =
		in->vertexes[1].lighting =
		in->vertexes[2].lighting =
		in->vertexes[3].lighting = b->lighting;

		in->blend_depth = x_depth;

		frac += step;
		step = 1.f - frac;
	}
}

/**
 * @brief Generate sprite instances from sprites and beams, and update the vertex array.
 */
void R_UpdateSprites(r_view_t *view) {

	R_AddBspLightgridSprites(view);
	
	const r_sprite_t *s = view->sprites;
	for (int32_t i = 0; i < view->num_sprites; i++, s++) {
		R_UpdateSprite(view, s);
	}

	const r_beam_t *b = view->beams;
	for (int32_t i = 0; i < view->num_beams; i++, b++) {
		R_UpdateBeam(view, b);
	}

	g_hash_table_remove_all(r_sprites.blend_depth_hash);

	r_sprite_instance_t *in = view->sprite_instances;
	for (int32_t i = 0; i < view->num_sprite_instances; i++, in++) {

		r_sprite_instance_t *chain = g_hash_table_lookup(r_sprites.blend_depth_hash, GINT_TO_POINTER(in->blend_depth));
		if (chain == NULL) {
			g_hash_table_insert(r_sprites.blend_depth_hash, GINT_TO_POINTER(in->blend_depth), in);
			in->tail = in->head = in;
		} else {
			in->prev = chain->tail;
			chain->tail = in;
			in->prev->next = in;
		}
	}
}

/**
 * @brief
 */
void R_DrawSprites(const r_view_t *view, int32_t blend_depth) {

	assert(view->framebuffer);

	glDepthMask(GL_FALSE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(r_sprite_program.name);

	glBindVertexArray(r_sprites.vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_sprites.vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_sprites.elements_buffer);

	glBufferSubData(GL_ARRAY_BUFFER, 0, view->num_sprite_instances * sizeof(r_sprite_vertex_t) * 4, r_sprites.vertexes);
	
	glEnableVertexAttribArray(r_sprite_program.in_position);
	glEnableVertexAttribArray(r_sprite_program.in_diffusemap);
	glEnableVertexAttribArray(r_sprite_program.in_color);
	glVertexAttrib1f(r_sprite_program.in_lerp, 0.f);
	glEnableVertexAttribArray(r_sprite_program.in_softness);
	glEnableVertexAttribArray(r_sprite_program.in_lighting);

	if (r_sprite_soften->value) {
		R_CopyFramebufferAttachment(view->framebuffer, ATTACHMENT_DEPTH, &r_sprites.depth_attachment);
	}

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DEPTH_STENCIL_ATTACHMENT);
	glBindTexture(GL_TEXTURE_2D, r_sprites.depth_attachment);

	glEnable(GL_DEPTH_TEST);

	r_sprite_instance_t *in = g_hash_table_lookup(r_sprites.blend_depth_hash, GINT_TO_POINTER(blend_depth));
	
	if (in) {
		in = in->head;
	}

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

		if (in->flags & SPRITE_NO_DEPTH) {
			glDepthRange(.1f, .9f);
		} else {
			glDepthRange(0.f, 1.f);
		}

		GLsizei count = 0;

		r_sprite_instance_t *chain;
		for (chain = in; chain; chain = chain->next) {

			if (chain->index == in->index + count &&
				chain->diffusemap == in->diffusemap &&
				chain->next_diffusemap == in->next_diffusemap &&
				(chain->flags & SPRITE_NO_DEPTH) == (in->flags & SPRITE_NO_DEPTH)) {
				count++;
			} else {
				break;
			}
		}

		glDrawElements(GL_TRIANGLES, count * 6, GL_UNSIGNED_INT, (GLvoid *) (in->index * sizeof(GLuint) * 6));
		r_stats.sprite_draw_elements++;

		in = chain;
	}

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);

	glDepthRange(0.f, 1.f);

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
			R_ShaderDescriptor(GL_VERTEX_SHADER, "lightgrid.glsl", "sprite_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "lightgrid.glsl", "soften_fs.glsl", "sprite_fs.glsl", NULL),
			NULL);
	
	glUseProgram(r_sprite_program.name);

	r_sprite_program.uniforms_block = glGetUniformBlockIndex(r_sprite_program.name, "uniforms_block");
	glUniformBlockBinding(r_sprite_program.name, r_sprite_program.uniforms_block, 0);

	r_sprite_program.lights_block = glGetUniformBlockIndex(r_sprite_program.name, "lights_block");
	glUniformBlockBinding(r_sprite_program.name, r_sprite_program.lights_block, 1);
	
	r_sprite_program.in_position = glGetAttribLocation(r_sprite_program.name, "in_position");
	r_sprite_program.in_diffusemap = glGetAttribLocation(r_sprite_program.name, "in_diffusemap");
	r_sprite_program.in_next_diffusemap = glGetAttribLocation(r_sprite_program.name, "in_next_diffusemap");
	r_sprite_program.in_color = glGetAttribLocation(r_sprite_program.name, "in_color");
	r_sprite_program.in_lerp = glGetAttribLocation(r_sprite_program.name, "in_lerp");
	r_sprite_program.in_softness = glGetAttribLocation(r_sprite_program.name, "in_softness");
	r_sprite_program.in_lighting = glGetAttribLocation(r_sprite_program.name, "in_lighting");

	r_sprite_program.texture_diffusemap = glGetUniformLocation(r_sprite_program.name, "texture_diffusemap");
	r_sprite_program.texture_lightgrid_ambient = glGetUniformLocation(r_sprite_program.name, "texture_lightgrid_ambient");
	r_sprite_program.texture_lightgrid_diffuse = glGetUniformLocation(r_sprite_program.name, "texture_lightgrid_diffuse");
	r_sprite_program.texture_lightgrid_fog = glGetUniformLocation(r_sprite_program.name, "texture_lightgrid_fog");
	r_sprite_program.texture_next_diffusemap = glGetUniformLocation(r_sprite_program.name, "texture_next_diffusemap");
	r_sprite_program.texture_depth_stencil_attachment = glGetUniformLocation(r_sprite_program.name, "texture_depth_stencil_attachment");

	glUniform1i(r_sprite_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);
	glUniform1i(r_sprite_program.texture_lightgrid_ambient, TEXTURE_LIGHTGRID_AMBIENT);
	glUniform1i(r_sprite_program.texture_lightgrid_diffuse, TEXTURE_LIGHTGRID_DIFFUSE);
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
	glBufferData(GL_ARRAY_BUFFER, sizeof(r_sprites.vertexes), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, diffusemap));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, next_diffusemap));
	glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, color));
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, lerp));
	glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, softness));
	glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, lighting));

	glGenBuffers(1, &r_sprites.elements_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_sprites.elements_buffer);

	GLuint elements[MAX_SPRITES * 6];
	for (GLuint i = 0, v = 0, e = 0; i < MAX_SPRITES; i++, v += 4, e += 6) {
		elements[e + 0] = v + 0;
		elements[e + 1] = v + 1;
		elements[e + 2] = v + 2;
		elements[e + 3] = v + 0;
		elements[e + 4] = v + 2;
		elements[e + 5] = v + 3;
	}
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	R_GetError(NULL);

	r_sprites.blend_depth_hash = g_hash_table_new(g_direct_hash, g_direct_equal);

	R_InitSpriteProgram();
	
	r_sprite_lerp = Cvar_Add("r_sprite_lerp", "1", 0, "Whether animated sprites linearly interpolate their images");
	r_sprite_soften = Cvar_Add("r_sprite_soften", "1", 0, "Whether sprite softening is enabled");
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
	glDeleteBuffers(1, &r_sprites.elements_buffer);

	g_hash_table_destroy(r_sprites.blend_depth_hash);

	if (r_sprites.depth_attachment) {
		glDeleteTextures(1, &r_sprites.depth_attachment);
	}

	R_ShutdownSpriteProgram();
}
