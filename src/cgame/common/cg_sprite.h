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

#if defined(__CG_LOCAL_H__)

#define SPRITE_GRAVITY 180.f

typedef struct ClientGameSprite ClientGameSprite;

/**
 * @brief Sprite types.
 */
typedef enum {

  /**
   * @brief A normal sprite, billboard or directional.
   */
  SPRITE_NORMAL  = 0,

  /**
   * @brief A beam, or segmented sprite with origin and termination points.
   */
  SPRITE_BEAM    = 1,

} ClientGameSpriteType;

/**
 * @brief Sprite think function type.
 */
typedef void (*Cg_SpriteThink)(ClientGameSprite *sprite, float life, float delta);

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

/**
 * @brief Type-safe mapping to entity & spawn ID
 */
typedef struct {
  int16_t entityId;
  uint8_t spawnId;
} ClientGameSpriteEntity;

/**
 * @brief Convenience function to get a `ClientGameSpriteEntity` from a `ClientEntity`
 * @param ent The entity to get a sprite entity for
 * @return The sprite entity
 */
static inline ClientGameSpriteEntity Cg_GetSpriteEntity(const ClientEntity *ent) {
  return (ClientGameSpriteEntity) {
    .entityId = ent->current.number,
    .spawnId = ent->current.spawnId
  };
}

/**
 * @brief Client game sprites can persist over multiple frames.
 */
struct ClientGameSprite {

  /**
   * @brief Type of sprite.
   */
  ClientGameSpriteType type;

  /**
   * @brief The sprite origin.
   */
  Vec3 origin;

  /**
   * @brief The sprite termination, for beams.
   */
  Vec3 termination;

  /**
   * @brief The sprite velocity.
   */
  Vec3 velocity;

  /**
   * @brief The sprite acceleration.
   */
  Vec3 acceleration;

  /**
   * @brief The sprite friction.
   */
  float friction;

  /**
   * @brief The sprite rotation, in radians.
   */
  float rotation;

  /**
   * @brief The sprite rotation velocity.
   */
  float rotationVelocity;

  /**
   * @brief The sprite direction. { 0, 0, 0 } is billboard.
   */
  Vec3 dir;

  /**
   * @brief The sprite color.
   */
  Vec3 color;

  /**
   * @brief The sprite's end color.
   * @see color
   */
  Vec3 endColor;

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
  float sizeVelocity;

  /**
   * @brief The sprite size acceleration.
   */
  float sizeAcceleration;

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
   * @brief Think function for custom logic.
   */
  Cg_SpriteThink Think;

  /**
   * @brief Custom data allocated on a sprite. Automatically freed unless
   * `SPRITE_DATA_NOFREE` is set.
   */
  void *data;

  /**
   * @brief The sprite's media.
   */
  union {
    RenderMedia *media;
    RenderImage *image;
    RenderAtlasImage *atlasImage;
    RenderAnimation *animation;
  };

  /**
   * @brief Sprite flags.
   */
  RenderSpriteFlags flags;

  /**
   * @brief Sprite billboard axis.
   */
  RenderSpriteBillboardAxis axis;

  /**
   * @brief Sprite lighting mix factor. 0 is fullbright, 1 is fully affected by light.
   */
  float lighting;

  /**
   * @brief Entity to follow, for `SPRITE_FOLLOW_ENTITY`. Use `Cg_GetSpriteEntity`.
   */
  ClientGameSpriteEntity entity;

  ClientGameSprite *prev;
  ClientGameSprite *next;
};

/**
 * @brief Calculate a lifetime value that causes the animation to run at a specified framerate.
 */
static inline uint32_t Cg_AnimationLifetime(const RenderAnimation *animation, const float fps) {
  return animation->numFrames * FRAMES_TO_SECONDS(fps);
}

ClientGameSprite *Cg_AddSprite(const ClientGameSprite *inS);
ClientGameSprite *Cg_FreeSprite(ClientGameSprite *p);
void Cg_FreeSpritesByData(const void *data);
void Cg_FreeSprites(void);
void Cg_AddSprites(void);
#endif
