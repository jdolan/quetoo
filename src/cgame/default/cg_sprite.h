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

/**
 * @brief Sprite types.
 */
typedef enum {
	/**
	 * @brief A normal sprite, billboard or directional.
	 */
	SPRITE_NORMAL	= 0,

	/**
	 * @brief A beam.
	 */
	SPRITE_BEAM		= 1
} cg_sprite_type_t;

typedef float (*cg_easing_function_t) (float life);

typedef struct cg_sprite_s cg_sprite_t;

typedef void (*cg_sprite_think_t) (cg_sprite_t *sprite, float life, float delta);


/**
 * @brief CGame-specific sprite flags.
 */
enum {
	/**
	 * @brief Beam's velocity does not affect the end point.
	 */
	SPRITE_BEAM_VELOCITY_NO_END = SPRITE_CGAME,

	/**
	 * @brief Life time is calculated based on server time rather than client time.
	 */
	SPRITE_SERVER_TIME = SPRITE_CGAME << 1,

	/**
	 * @brief Data is not heap-allocated, so don't free.
	 */
	SPRITE_DATA_NOFREE = SPRITE_CGAME << 2,

	/**
	 * @brief Sprite is relative to entity ID specified in the structure.
	 */
	SPRITE_FOLLOW_ENTITY = SPRITE_CGAME << 3,

	/**
	 * @brief Rather than despawning, the sprite will "unlink" from its entity when it dies
	 */
	SPRITE_ENTITY_UNLINK_ON_DEATH = SPRITE_CGAME << 4
};

typedef uint32_t cg_r_sprite_flags_t;

/**
 * @brief Type-safe mapping to entity & spawn ID
 */
typedef struct {
	uint16_t entity_id;
	uint8_t spawn_id;
} cg_sprite_entity_t;

/**
 * @brief Convenience function to get a `cg_sprite_entity_t` from a `cl_entity_t`
 * @param ent The entity to get a sprite entity for
 * @return The sprite entity
 */
static inline cg_sprite_entity_t Cg_GetSpriteEntity(cl_entity_t *ent) {
	return (cg_sprite_entity_t) {
		.entity_id = ent->current.number,
		.spawn_id = ent->current.spawn_id
	};
}

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
	 * @brief The sprite termination, for beams.
	 */
	vec3_t termination;

	/**
	 * @brief The sprite velocity.
	 */
	vec3_t velocity;

	/**
	 * @brief The sprite acceleration.
	 */
	vec3_t acceleration;

	/**
	 * @brief The sprite rotation, in radians.
	 */
	float rotation;

	/**
	 * @brief The sprite rotation velocity.
	 */
	float rotation_velocity;

	/**
	 * @brief The sprite direction. { 0, 0, 0 } is billboard.
	 */
	vec3_t dir;

	/**
	 * @brief The sprite color, in HSVA.
	 * @details For alpha-blended sprites, the colors are used as "normal". For additive sprites,
	 * alpha *must* be zero at all times.
	 */
	vec4_t color;

	/**
	 * @brief The sprite's end color, in HSVA.
	 * @see color
	 */
	vec4_t end_color;

	/**
	 * @brief The sprite size, in world units. If this is specified, width/height are not used.
	 */
	float size;

	/**
	 * @brief The sprite width, in world units.
	 */
	float width;

	/**
	 * @brief The sprite height, in world units.
	 */
	float height;

	/**
	 * @brief The sprite size velocity.
	 */
	float size_velocity;

	/**
	 * @brief The sprite size acceleration.
	 */
	float size_acceleration;

	/**
	 * @brief The sprite bounce factor.
	 */
	float bounce;

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

	/**
	 * @brief Easing function for life time.
	 */
	cg_easing_function_t life_easing;

	/**
	 * @brief Think function for particle.
	 */
	cg_sprite_think_t think;

	/**
	 * @brief Custom data allocated on a sprite. Automatically freed unless
	 * SPRITE_DATA_NOFREE is set.
	 */
	void *data;

	/**
	 * @brief The sprite's media.
	 */
	union {
		r_media_t *media;
		r_image_t *image;
		r_atlas_image_t *atlas_image;
		r_animation_t *animation;
	};

	/**
	 * @brief Sprite flags.
	 */
	cg_r_sprite_flags_t flags;

	/**
	 * @brief Sprite billboard axis.
	 */
	r_sprite_billboard_axis_t axis;
	
	/**
	 * @brief Sprite softness scalar. Negative values invert the scalar.
	 */
	float softness;

	/**
	 * @brief Sprite lighting mix factor. 0 is fullbright, 1 is fully affected by light.
	 */
	float lighting;

	/**
	 * @brief Entity to follow, for SPRITE_FOLLOW_ENTITY. Use Cg_GetSpriteEntity.
	 */
	cg_sprite_entity_t entity;

	cg_sprite_t *prev;
	cg_sprite_t *next;
} cg_sprite_t;

/**
 * @brief Calculate a lifetime value that causes the animation to run at a specified framerate.
 */
static inline uint32_t Cg_AnimationLifetime(const r_animation_t *animation, const float fps) {
	return animation->num_frames * FRAMES_TO_SECONDS(fps);
}

cg_sprite_t *Cg_AddSprite(const cg_sprite_t *in_s);
cg_sprite_t *Cg_FreeSprite(cg_sprite_t *p);
void Cg_FreeSprites(void);
void Cg_AddSprites(void);
#endif /* __CG_LOCAL_H__ */
