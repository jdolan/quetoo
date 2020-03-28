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

#pragma once

#ifdef __CG_LOCAL_H__

#define SPRITE_GRAVITY 180.f

typedef enum {
	/**
	 * @brief A normal billboard sprite.
	 */
	SPRITE_NORMAL	= 0,

	/**
	 * @brief A beam.
	 */
	SPRITE_BEAM		= 1
} cg_sprite_type_t;

/**
 * @brief Client game sprites can persist over multiple frames.
 */
typedef struct cg_sprite_s {

	/**
	 * @brief Type of sprite.
	 */
	cg_sprite_type_t type;

	/**
	 * @brief The sprite origin.
	 */
	vec3_t origin;

	/**
	 * @brief The sprite velocity.
	 */
	vec3_t velocity;

	/**
	 * @brief The sprite acceleration.
	 */
	vec3_t acceleration;

	union {
		struct {
			/**
			 * @brief The sprite rotation.
			 */
			float rotation;
	
			/**
			 * @brief The sprite rotation velocity.
			 */
			float rotation_velocity;
		};

		struct {
			/**
			 * @brief Beam end position.
			 */
			vec3_t end;
		} beam;
	};

	/**
	 * @brief The sprite color.
	 */
	color_t color;

	/**
	 * @brief The sprite's end color.
	 */
	color_t end_color;

	/**
	 * @brief Color transition
	 */
	cg_transition_point_t *color_transition;

	/**
	 * @brief Color transition point count
	 */
	size_t color_transition_count;

	/**
	 * @brief Blending operators
	 */
	GLenum dst, src;
	
	/**
	 * @brief Sprite's image.
	 */
	union {
		r_media_t *media;
		r_image_t *image;
		r_atlas_image_t *atlas_image;
		r_animation_t *animation;
	};

	/**
	 * @brief The sprite size, in world units.
	 */
	float size;

	/**
	 * @brief The sprite size velocity.
	 */
	float size_velocity;

	/**
	 * @brief The sprite size acceleration.
	 */
	float size_acceleration;

	/**
	 * @brief The client time when the sprite was allocated.
	 */
	uint32_t time;

	/**
	 * @brief The lifetime, after which point it decays.
	 */
	uint32_t lifetime;

	/**
	 * @brief The time when this sprite was last updated.
	 */
	uint32_t timestamp;

	struct cg_sprite_s *prev;
	struct cg_sprite_s *next;
} cg_sprite_t;

cg_sprite_t *Cg_AllocSprite(void);
cg_sprite_t *Cg_FreeSprite(cg_sprite_t *p);
void Cg_FreeSprites(void);
void Cg_AddSprites(void);
#endif /* __CG_LOCAL_H__ */
