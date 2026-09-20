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

static CGameSprite *cgFreeSprites;
static CGameSprite *cgActiveSprites;

static CGameSprite cgSprites[MAX_SPRITES];

/**
 * @brief Pushes the sprite onto the head of specified list.
 */
static void Cg_PushSprite(CGameSprite *s, CGameSprite **list) {

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
static void Cg_PopSprite(CGameSprite *s, CGameSprite **list) {

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
CGameSprite *Cg_AddSprite(const CGameSprite *inS) {

  if (!cg_addSprites->integer) {
    return NULL;
  }

  if (!cgFreeSprites) {
    Cg_Debug("No free sprites\n");
    return NULL;
  }

  assert(inS->media);

  CGameSprite *s = cgFreeSprites;

  Cg_PopSprite(s, &cgFreeSprites);

  *s = *inS;

  if (inS->flags & SPRITE_SERVER_TIME) {
    s->time = s->timestamp = cgi.client->frame.time;
  } else {
    s->time = s->timestamp = cgi.client->unclampedTime;
  }

  Cg_PushSprite(s, &cgActiveSprites);

  return s;
}

/**
 * @brief Frees the specified sprite, returning the sprite it was pointing
 * to as a convenience for continued iteration.
 */
CGameSprite *Cg_FreeSprite(CGameSprite *s) {
  CGameSprite *next = s->next;

  Cg_PopSprite(s, &cgActiveSprites);

  Cg_PushSprite(s, &cgFreeSprites);

  if (s->data && !(s->flags & SPRITE_DATA_NOFREE)) {
    cgi.Free(s->data);
    s->data = NULL;
  }

  return next; 
}

/**
 * @brief Frees all active sprites whose data pointer matches the given pointer.
 * @details Used to purge sprites that hold a reference to data being freed,
 *   without disturbing unrelated sprites.
 */
void Cg_FreeSpritesByData(const void *data) {

  CGameSprite *s = cgActiveSprites;
  while (s) {
    if (s->data == data) {
      s->flags |= SPRITE_DATA_NOFREE;
      s = Cg_FreeSprite(s);
    } else {
      s = s->next;
    }
  }
}

/**
 * @brief Frees all sprites, returning them to the eligible list.
 */
void Cg_FreeSprites(void) {

  cgFreeSprites = NULL;
  cgActiveSprites = NULL;

  memset(cgSprites, 0, sizeof(cgSprites));

  for (size_t i = 0; i < lengthof(cgSprites); i++) {
    Cg_PushSprite(&cgSprites[i], &cgFreeSprites);
  }
}

/**
 * @brief Adds all sprites that are active for this frame to the view.
 */
void Cg_AddSprites(void) {

  if (!cg_addSprites->integer) {
    return;
  }

  const float delta = MILLIS_TO_SECONDS(cgi.client->frameMsec);
  const uint32_t clientTime = cgi.client->unclampedTime, serverTime = cgi.client->frame.time;

  CGameSprite *s = cgActiveSprites;
  while (s) {

    assert(s->media);

    const uint32_t time = (s->flags & SPRITE_SERVER_TIME) ? serverTime : clientTime;

    const float life = (time - s->time) / (float) (s->lifetime ?: 1);

    if (s->Think) {
      s->Think(s, life, delta);
    }

    if (s->time != time) {
      if (life >= 1.f) {
        s = Cg_FreeSprite(s);
        continue;
      }
    }

    ClientEntity *entity = NULL;
    if (s->flags & SPRITE_FOLLOW_ENTITY) {
      entity = &cgi.client->entities[s->entity.entityId];

      if (entity->frameNum != cgi.client->frame.frameNum ||
        entity->current.spawnId != s->entity.spawnId) {

        if (!(s->flags & SPRITE_ENTITY_UNLINK_ON_DEATH) || entity->prev.spawnId != s->entity.spawnId) {
          s = Cg_FreeSprite(s);
          continue;
        }

        s->flags &= ~(SPRITE_FOLLOW_ENTITY | SPRITE_ENTITY_UNLINK_ON_DEATH);
        s->origin = Vec3_Add(s->origin, entity->previousOrigin);

        if (s->type == SPRITE_BEAM) {
          s->termination = Vec3_Add(s->termination, entity->previousOrigin);
        }
      }
    }

    s->sizeVelocity += s->sizeAcceleration * delta;

    if (s->size) {
      s->size += s->sizeVelocity * delta;

      if (s->size <= 0.f) {
        s = Cg_FreeSprite(s);
        continue;
      }
    } else {
      s->width += s->sizeVelocity * delta;
      s->height += s->sizeVelocity * delta;

      if (s->width <= 0.f || s->height <= 0.f) {
        s = Cg_FreeSprite(s);
        continue;
      }
    }

    Vec3 oldOrigin = s->origin;

    s->velocity = Vec3_Fmaf(s->velocity, delta, s->acceleration);

    const float speed = Maxf(1.f, Vec3_Length(s->velocity));
    const float deceleration = Maxf(0.f, speed - s->friction * delta) / speed;
    s->velocity = Vec3_Scale(s->velocity, deceleration);

    s->origin = Vec3_Fmaf(s->origin, delta, s->velocity);

    if (s->bounce && cg_spritePhysics->integer) {
      Vec3 origin = s->origin;

      if (s->flags & SPRITE_FOLLOW_ENTITY) {
        oldOrigin = Vec3_Add(oldOrigin, entity->origin);
        origin = Vec3_Add(origin, entity->origin);
      }

      const float size = s->size ?: max(s->height, s->width);
      const Box3 bounds = Box3f(size, size, size);
      CmTrace tr = cgi.Trace(oldOrigin, origin, bounds, NULL, CONTENTS_MASK_SOLID);

      if (tr.startSolid || tr.allSolid) {
        tr = cgi.Trace(oldOrigin, origin, Box3_Zero(), NULL, CONTENTS_MASK_SOLID);
      }

      if (tr.fraction < 1.0) {
        s->velocity = Vec3_Scale(Vec3_Reflect(s->velocity, tr.plane.normal), s->bounce);
        s->origin = tr.end;
        
        if (s->flags & SPRITE_FOLLOW_ENTITY) {
          s->origin = Vec3_Subtract(s->origin, entity->origin);
        }
      }
    }

    const Vec3 color = Vec3_Mix(s->color, s->endColor, life);

    Vec3 origin = s->origin;
    if (s->flags & SPRITE_FOLLOW_ENTITY) {
      origin = Vec3_Add(origin, entity->origin);
    }

    switch (s->type) {
      case SPRITE_NORMAL:
        s->rotation += s->rotationVelocity * delta;

        cgi.AddSprite(cgi.view, &(RenderSprite) {
          .origin = origin,
          .size = s->size,
          .width = s->width,
          .height = s->height,
          .color = color,
          .rotation = s->rotation,
          .media = s->media,
          .life = life,
          .flags = s->flags,
          .dir = s->dir,
          .axis = s->axis,
          .lighting = s->lighting,
        });
        break;
      case SPRITE_BEAM: {
        if (!(s->flags & SPRITE_BEAM_VELOCITY_NO_END)) {
          s->termination = Vec3_Fmaf(s->termination, delta, s->velocity);
        }

        Vec3 termination = s->termination;

        if (s->flags & SPRITE_FOLLOW_ENTITY) {
          termination = Vec3_Add(termination, entity->origin);
        }

        cgi.AddBeam(cgi.view, &(RenderBeam) {
          .start = origin,
          .end = termination,
          .size = s->size,
          .image = (RenderImage *) s->image,
          .color = color,
          .flags = s->flags,
          .lighting = s->lighting,
        });
        break;
      }
    }
    
    s = s->next;
  }
}
