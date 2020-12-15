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
	vec2_t next_diffusemap;
	color32_t color;
	float lerp;
	int32_t blend_depth;
} r_sprite_vertex_t;

/**
 * @brief
 */
static struct {
	r_sprite_vertex_t sprites[MAX_SPRITES * 4];

	GLuint vertex_array;
	GLuint vertex_buffer;
	GLuint index_buffer;

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
	GLint in_blend_depth;

	GLint blend_depth;

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
static _Bool R_CullSprite(r_sprite_vertex_t *out) {

	vec3_t mins, maxs;

	Cm_TraceBounds(out[0].position, out[2].position, Vec3_Zero(), Vec3_Zero(), &mins, &maxs);

	return R_CullBox(mins, maxs);
}

/**
 * @brief
 */
static void R_AddSpriteInstance(const r_sprite_instance_t *in, const r_sprite_flags_t flags, float lerp, const color32_t color, r_sprite_vertex_t *out) {
	r_sprite_instance_t *last = &r_view.sprite_instances[r_view.num_sprite_instances - 1];

	const _Bool is_current_batch = r_view.num_sprite_instances &&
		in->diffusemap->texnum == last->diffusemap->texnum &&
		((in->next_diffusemap ? in->next_diffusemap->texnum : 0) == (last->next_diffusemap ? last->next_diffusemap->texnum : 0));

	R_SpriteTextureCoordinates(in->diffusemap, &out[0].diffusemap, &out[1].diffusemap, &out[2].diffusemap, &out[3].diffusemap);

	out->lerp = 0.f;

	if (in->next_diffusemap) {
		R_SpriteTextureCoordinates(in->next_diffusemap, &out[0].next_diffusemap, &out[1].next_diffusemap, &out[2].next_diffusemap, &out[3].next_diffusemap);
		if (lerp) {
			out->lerp = lerp;
		}
	}

	out[0].color = out[1].color = out[2].color = out[3].color = color;
	out[1].lerp = out[2].lerp = out[3].lerp = out[0].lerp;
	out[0].blend_depth = out[1].blend_depth = out[2].blend_depth = out[3].blend_depth = (flags & SPRITE_NO_BLEND_DEPTH) ? 0 : -1;

	if (is_current_batch) {
		last->count++;
	} else {
		r_view.sprite_instances[r_view.num_sprite_instances] = *in;
		r_view.sprite_instances[r_view.num_sprite_instances].count = 1;
		r_view.num_sprite_instances++;
	}

	r_view.num_sprites++;
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test.
 */
void R_AddSprite(const r_sprite_t *s) {

	if (r_view.num_sprites == MAX_SPRITES) {
		return;
	}

	const r_image_t *image = R_ResolveSpriteImage(s->media, s->life);
	const float aspect_ratio = (float) image->width / (float) image->height;
	r_sprite_vertex_t *out = r_sprites.sprites + (r_view.num_sprites * 4);
	const float size = s->size * .5f;
	
	vec3_t dir, right, up;
	
	if (Vec3_Equal(s->dir, Vec3_Zero())) {

		if (!s->axis || s->axis & (SPRITE_AXIS_X | SPRITE_AXIS_Y | SPRITE_AXIS_Z)) {
			if (s->rotation) {
				dir = r_view.angles;
				dir.z = Degrees(s->rotation);
				Vec3_Vectors(dir, NULL, &right, &up);
			} else {
				right = r_view.right;
				up = r_view.up;
			}
		} else {
			dir = Vec3_Zero();

			for (int32_t i = 0; i < 3; i++) {
				if (s->axis & (1 << i)) {
					dir.xyz[i] = r_view.forward.xyz[i];
				}
			}

			dir = Vec3_Euler(dir);
			dir.z = Degrees(s->rotation);
			Vec3_Vectors(dir, NULL, &right, &up);
		}
	} else {
		dir = Vec3_Euler(s->dir);
		dir.z = Degrees(s->rotation);
		Vec3_Vectors(dir, NULL, &right, &up);
	}

	const vec3_t u = Vec3_Scale(up, size), d = Vec3_Scale(up, -size), l = Vec3_Scale(right, -size * aspect_ratio), r = Vec3_Scale(right, size * aspect_ratio);
	
	out[0].position = Vec3_Add(Vec3_Add(s->origin, u), l);
	out[1].position = Vec3_Add(Vec3_Add(s->origin, u), r);
	out[2].position = Vec3_Add(Vec3_Add(s->origin, d), r);
	out[3].position = Vec3_Add(Vec3_Add(s->origin, d), l);

	if (R_CullSprite(out)) {
		return;
	}

	const r_image_t *next_image = NULL;
	float lerp = 0.f;

	if (s->media->type == R_MEDIA_ANIMATION) {
		const r_animation_t *anim = (const r_animation_t *) s->media;

		next_image = R_ResolveAnimation(anim, s->life, 1);

		if (s->flags & SPRITE_LERP) {
			const float life_to_images = floorf(s->life * anim->num_frames);
			const float cur_frame = life_to_images / anim->num_frames;
			const float next_frame = (life_to_images + 1) / anim->num_frames;
			lerp = (s->life - cur_frame) / (next_frame - cur_frame);
		}
	}

	R_AddSpriteInstance(&(const r_sprite_instance_t) {
		.diffusemap = image,
		.next_diffusemap = next_image,
	}, s->flags, lerp, s->color, out);
}

#define MAX_BEAMS				512
#define MAX_BEAM_SEGMENTS		32

typedef struct {
	float start, end;
	int32_t start_blend_depth;
} r_beam_segment_t;

/**
 * @brief
 */
static int32_t R_BeamSegmentCompare(const void *a, const void *b) {
	
	const r_beam_segment_t *beam_a = (const r_beam_segment_t *) a;
	const r_beam_segment_t *beam_b = (const r_beam_segment_t *) b;

	assert(beam_a->end && beam_b->end);
	
	if (beam_a->start < beam_b->start) {
		return -1;
	}

	return 1;
}

static r_beam_t beams[MAX_BEAMS];
static int32_t num_beams;

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test.
 */
void R_AddBeam(const r_beam_t *b) {

	if (num_beams == MAX_BEAMS) {
		return;
	}

	vec3_t mins, maxs;
	Cm_TraceBounds(b->start, b->end, Vec3_Zero(), Vec3_Zero(), &mins, &maxs);

	if (R_CullBox(mins, maxs)) {
		return;
	}

	beams[num_beams] = *b;
	num_beams++;
}

/**
 * @brief
 */
static void R_CreateBeams(void) {

	r_beam_t *b = beams;

	for (int32_t i = 0; i < num_beams; i++, b++) {
	
		const r_image_t *image = R_ResolveSpriteImage((r_media_t *) b->image, 0);
		r_sprite_vertex_t *out = r_sprites.sprites + (r_view.num_sprites * 4);
		const float size = b->size * .5f;
		float length;
		const vec3_t up = Vec3_NormalizeLength(Vec3_Subtract(b->start, b->end), &length);
		length /= b->image->width * (b->size / b->image->height);
		const vec3_t right = Vec3_Scale(Vec3_Normalize(Vec3_Cross(up, Vec3_Subtract(r_view.origin, b->end))), size);

		// construct segments
		r_beam_segment_t segments[MAX_BEAM_SEGMENTS] = {
			{ .start = 0, .end = 1, .start_blend_depth = R_BlendDepthForPoint(b->start, BLEND_DEPTH_SPRITE) }
		}, *free_segment = segments + 1;
	
		for (r_beam_segment_t *segment = segments; segment < free_segment && free_segment != segments + MAX_BEAM_SEGMENTS && (r_view.num_sprites + (free_segment - segment)) < MAX_SPRITES; ) {

			const int32_t end_blend_depth = R_BlendDepthForPoint(Vec3_Mix(b->start, b->end, segment->end), BLEND_DEPTH_SPRITE);

			if (segment->start_blend_depth == end_blend_depth) {
				segment++;
				continue;
			}

			*free_segment = *segment;
			segment->end *= .5f;
			free_segment->start = segment->end;
			free_segment->start_blend_depth = end_blend_depth;
		
			free_segment++;
		}

		// sort segments
		qsort(segments, (free_segment - segments), sizeof(r_beam_segment_t), R_BeamSegmentCompare);

		vec2_t texcoords[4];

		R_SpriteTextureCoordinates(image, &texcoords[0], &texcoords[1], &texcoords[2], &texcoords[3]);
	
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

		// add segments
		for (const r_beam_segment_t *segment = segments; segment != free_segment; segment++) {
	
			const vec3_t start = Vec3_Mix(b->start, b->end, segment->start);
			const vec3_t end = Vec3_Mix(b->start, b->end, segment->end);

			out[0].position = Vec3_Add(start, right);
			out[1].position = Vec3_Add(end, right);
			out[2].position = Vec3_Subtract(end, right);
			out[3].position = Vec3_Subtract(start, right);

			if (R_CullSprite(out)) {
				continue;
			}

			R_AddSpriteInstance(&(const r_sprite_instance_t) {
				.diffusemap = image,
				.next_diffusemap = NULL
			}, 0, 0, b->color, out);
		
			const float start_diffuse = (texcoords[0].x * (1.f - segment->start)) + (texcoords[1].x * segment->start);
			const float end_diffuse = (texcoords[0].x * (1.f - segment->end)) + (texcoords[1].x * segment->end);
		
			out[0].diffusemap.x = start_diffuse;
			out[1].diffusemap.x = end_diffuse;
			out[2].diffusemap.x = end_diffuse;
			out[3].diffusemap.x = start_diffuse;

			out[0].blend_depth = out[1].blend_depth = out[2].blend_depth = out[3].blend_depth = segment->start_blend_depth;

			out += 4;
		}
	}

	num_beams = 0;
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

	r_sprite_vertex_t *out = r_sprites.sprites;
	for (int32_t i = 0; i < r_view.num_sprites; i++, out += 4) {

		vec3_t center = Vec3_Zero();
		for (int32_t j = 0; j < 4; j++) {
			center = Vec3_Add(center, out[j].position);
		}
		center = Vec3_Scale(center, .25f);

		out[0].blend_depth =
		out[1].blend_depth =
		out[2].blend_depth =
		out[3].blend_depth = R_BlendDepthForPoint(center, BLEND_DEPTH_SPRITE);
	}

	R_CreateBeams();

	glBindBuffer(GL_ARRAY_BUFFER, r_sprites.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, r_view.num_sprites * sizeof(r_sprite_vertex_t) * 4, r_sprites.sprites, GL_DYNAMIC_DRAW);

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

	glUniform1i(r_sprite_program.blend_depth, blend_depth);

	glBindVertexArray(r_sprites.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_sprites.vertex_buffer);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_sprites.index_buffer);
	
	glEnableVertexAttribArray(r_sprite_program.in_position);
	glEnableVertexAttribArray(r_sprite_program.in_diffusemap);
	glEnableVertexAttribArray(r_sprite_program.in_color);
	glVertexAttrib1f(r_sprite_program.in_lerp, 0.f);
	glEnableVertexAttribArray(r_sprite_program.in_blend_depth);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTGRID_FOG);
	glBindTexture(GL_TEXTURE_3D, r_world_model->bsp->lightgrid->textures[3]->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DEPTH_STENCIL_ATTACHMENT);
	glBindTexture(GL_TEXTURE_2D, r_context.depth_stencil_attachment);

	ptrdiff_t offset = 0;
	const r_sprite_instance_t *s = r_view.sprite_instances;
	for (int32_t i = 0; i < r_view.num_sprite_instances; i++, s++) {

		if (s->next_diffusemap) {
			glActiveTexture(GL_TEXTURE0 + TEXTURE_NEXT_DIFFUSEMAP);
			glBindTexture(GL_TEXTURE_2D, s->next_diffusemap->texnum);

			glEnableVertexAttribArray(r_sprite_program.in_next_diffusemap);
			glEnableVertexAttribArray(r_sprite_program.in_lerp);
		} else {
			glDisableVertexAttribArray(r_sprite_program.in_next_diffusemap);
			glDisableVertexAttribArray(r_sprite_program.in_lerp);
		}

		glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);
		glBindTexture(GL_TEXTURE_2D, s->diffusemap->texnum);

		glDrawElements(GL_TRIANGLES, s->count * 6, GL_UNSIGNED_INT, (GLvoid *) offset);
		offset += (s->count * 6) * sizeof(GLuint);
	}
	
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
			R_ShaderDescriptor(GL_GEOMETRY_SHADER, "sprite_gs.glsl", NULL),
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
	r_sprite_program.in_blend_depth = glGetAttribLocation(r_sprite_program.name, "in_blend_depth");

	r_sprite_program.blend_depth = glGetUniformLocation(r_sprite_program.name, "blend_depth");

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
	glVertexAttribIPointer(5, 1, GL_INT, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, blend_depth));

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
