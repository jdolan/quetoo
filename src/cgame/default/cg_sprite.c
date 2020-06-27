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

#include "cg_local.h"

static cg_sprite_t *cg_free_sprites;
static cg_sprite_t *cg_active_sprites;

static cg_sprite_t cg_sprites[MAX_SPRITES];

/**
 * @brief Pushes the sprite onto the head of specified list.
 */
static void Cg_PushSprite(cg_sprite_t *s, cg_sprite_t **list) {

	s->prev = NULL;

	if (*list) {
		(*list)->prev = s;
	}

	s->next = *list;
	*list = s;
}

/**
 * @brief Pops the sprite from the specified list, repairing the list if it
 * becomes broken.
 */
static void Cg_PopSprite(cg_sprite_t *s, cg_sprite_t **list) {

	if (s->prev) {
		s->prev->next = s->next;
	}

	if (s->next) {
		s->next->prev = s->prev;
	}

	if (*list == s) {
		*list = s->next;
	}

	s->prev = s->next = NULL;
}

/**
 * @brief Allocates a free sprite.
 */
cg_sprite_t *Cg_AddSprite(const cg_sprite_t *in_s) {

	if (!cg_add_particles->integer) {
		return NULL;
	}

	if (!cg_free_sprites) {
		cgi.Debug("No free sprites\n");
		return NULL;
	}

	assert(in_s->media);

	cg_sprite_t *s = cg_free_sprites;

	Cg_PopSprite(s, &cg_free_sprites);

	*s = *in_s;

	s->time = s->timestamp = cgi.client->unclamped_time;

	Cg_PushSprite(s, &cg_active_sprites);

	return s;
}

/**
 * @brief Frees the specified sprite, returning the sprite it was pointing
 * to as a convenience for continued iteration.
 */
cg_sprite_t *Cg_FreeSprite(cg_sprite_t *s) {
	cg_sprite_t *next = s->next;

	Cg_PopSprite(s, &cg_active_sprites);

	Cg_PushSprite(s, &cg_free_sprites);

	return next;
}

/**
 * @brief Frees all sprites, returning them to the eligible list.
 */
void Cg_FreeSprites(void) {

	cg_free_sprites = NULL;
	cg_active_sprites = NULL;

	memset(cg_sprites, 0, sizeof(cg_sprites));

	for (size_t i = 0; i < lengthof(cg_sprites); i++) {
		Cg_PushSprite(&cg_sprites[i], &cg_free_sprites);
	}
}

/**
 * @brief Adds all sprites that are active for this frame to the view.
 */
void Cg_AddSprites(void) {

	if (!cg_add_sprites->integer) {
		return;
	}

	const float delta = MILLIS_TO_SECONDS(cgi.client->frame_msec);

	cg_sprite_t *s = cg_active_sprites;
	while (s) {

		assert(s->media);

		if (s->time != cgi.client->unclamped_time) {
			if (cgi.client->unclamped_time - s->time > s->lifetime) {
				s = Cg_FreeSprite(s);
				continue;
			}
		}

		const uint32_t elapsed_time = (cgi.client->unclamped_time - s->time);
		float life = elapsed_time / (float)s->lifetime;

		if (s->life_easing)
			life = s->life_easing(life);

		s->size_velocity += s->size_acceleration * delta;
		s->size += s->size_velocity * delta;

		if (s->size <= 0.f) {
			s = Cg_FreeSprite(s);
			continue;
		}

		const vec3_t old_origin = s->origin;

		s->velocity = Vec3_Add(s->velocity, Vec3_Scale(s->acceleration, delta));
		s->origin = Vec3_Add(s->origin, Vec3_Scale(s->velocity, delta));

		if (s->bounce && cg_particle_quality->integer) {
			const float half_size = ceilf(s->size * .5f);
			cm_trace_t tr = cgi.Trace(old_origin, s->origin, Vec3(-half_size, -half_size, -half_size), Vec3(half_size, half_size, half_size), 0, CONTENTS_MASK_SOLID);

			if (tr.start_solid || tr.all_solid) {
				tr = cgi.Trace(old_origin, s->origin, Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_SOLID);
			}

			if (tr.fraction < 1.0) {
				s->velocity = Vec3_Scale(Vec3_Reflect(s->velocity, tr.plane.normal), s->bounce);
				s->origin = tr.end;
			}
		}

		const vec4_t lerped_color = Vec4_Mix(s->color, s->end_color, life);
		const color32_t color = Color_Color32(ColorHSVA(lerped_color.x, lerped_color.y, lerped_color.z, lerped_color.w));

		switch (s->type) {
			case SPRITE_NORMAL:
				s->rotation += s->rotation_velocity * delta;

				cgi.AddSprite(&(r_sprite_t) {
					.origin = s->origin,
					.size = s->size,
					.color = color,
					.rotation = s->rotation,
					.media = s->media,
					.life = life,
					.flags = (r_sprite_flags_t)s->flags,
					.dir = s->dir
				});
				break;
			case SPRITE_BEAM:
				s->termination = Vec3_Add(s->termination, Vec3_Scale(s->velocity, delta));

				cgi.AddBeam(&(r_beam_t) {
					.start = s->origin,
					.end = s->termination,
					.size = s->size,
					.image = (r_image_t *) s->image,
					.color = color,
				});
				break;
		}
		
		if (s->think)
			s->think(s, life, delta);
		
		s = s->next;
	}
}
