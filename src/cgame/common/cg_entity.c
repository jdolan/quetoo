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
#include "game/common/bg_pmove.h"

Vector *cg_entities = NULL;

/**
 * @brief The `Cg_EntityPredicate` type for `Cg_FindEntity`.
 */
typedef bool (*Cg_EntityPredicate)(const CmEntity *e, void *data);

/**
 * @return The first entity after `from` for which `predicate` returns `true`, or `NULL`.
 */
static const CmEntity *Cg_FindEntity(const CmEntity *from, const Cg_EntityPredicate predicate, void *data) {

  const CmBsp *bsp = cgi.WorldModel()->bsp->cm;

  int32_t start = 0;
  if (from) {
    while (start < bsp->numEntities) {
      if (bsp->entities[start] == from) {
        break;
      }
      start++;
    }
    assert(start < bsp->numEntities);
    start++;
  }

  for (int32_t i = start; i < bsp->numEntities; i++) {
    const CmEntity *e = bsp->entities[i];
    if (predicate(e, data)) {
      return e;
    }
  }

  return NULL;
}

/**
 * @brief Predicate function testing whether an entity's targetname matches the given string.
 */
static bool Cg_EntityTarget_Predicate(const CmEntity *e, void *data) {
  return !q_strcmp(cgi.EntityValue(e, "targetname")->nullableString, data);
}

/**
 * @brief Predicate function testing whether an entity's team key matches the given string.
 */
static bool Cg_EntityTeam_Predicate(const CmEntity *e, void *data) {
  return !q_strcmp(cgi.EntityValue(e, "team")->nullableString, data);
}

/**
 * @return The `ClientGameEntity *` for the specified `CmEntity *`, if any.
 */
ClientGameEntity *Cg_EntityForDefinition(const CmEntity *e) {

  if (e) {
    for (uint32_t i = 0; i < cg_entities->count; i++) {
      ClientGameEntity *ent = VectorElement(cg_entities, ClientGameEntity, i);
      if (ent->def == e) {
        return ent;
      }
    }
  }

  return NULL;
}

const ClientGameEntityClass *cg_entity_classes[] = {
  &cg_misc_dust,
  &cg_misc_flame,
  &cg_misc_model,
  &cg_misc_sound,
  &cg_misc_sparks,
  &cg_misc_sprite,
  &cg_misc_steam,
  &cg_misc_weather
};

const size_t cg_num_entity_classes = lengthof(cg_entity_classes);

/**
 * @brief Loads entities from the current level.
 * @remarks This should be called once per map load, after the precache routine.
 */
void Cg_LoadEntities(void) {

  Cg_FreeEntities();

  cg_entities = $(alloc(Vector), initWithSize, sizeof(ClientGameEntity));

  const CmBsp *bsp = cgi.WorldModel()->bsp->cm;
  for (int32_t i = 0; i < bsp->numEntities; i++) {

    const CmEntity *def = bsp->entities[i];
    const char *classname = cgi.EntityValue(def, "classname")->string;

    const ClientGameEntityClass **clazz = cg_entity_classes;
    for (size_t j = 0; j < cg_num_entity_classes; j++, clazz++) {

      if (!q_strcmp(classname, (*clazz)->classname)) {

        ClientGameEntity e = {
          .id = MAX_ENTITIES + (int32_t) cg_entities->count,
          .clazz = *clazz,
          .def = def
        };

        e.origin = cgi.EntityValue(def, "origin")->vec3;
        e.bounds = Box3_FromCenter(e.origin);

        if (cgi.EntityValue(def, "target")->parsed & ENTITY_STRING) {
          const char *targetName = cgi.EntityValue(def, "target")->string;
          e.target = Cg_FindEntity(NULL, Cg_EntityTarget_Predicate, (void *) targetName);
          if (!e.target) {
            Cg_Warn("Target not found for %s @ %s\n", classname, vtos(e.origin));
          }
        }

        if (cgi.EntityValue(def, "team")->parsed & ENTITY_STRING) {
          const char *teamName = cgi.EntityValue(def, "team")->string;
          e.team = Cg_FindEntity(def, Cg_EntityTeam_Predicate, (void *) teamName);
        }

        e.data = cgi.Malloc(e.clazz->dataSize, MEM_TAG_CGAME_LEVEL);

        e.clazz->Init(&e);

        // Reset periodic thinker scheduling so media reloads don't "catch up" from t=0
        // and spam emissions for several frames (e.g. misc_sound during r_restart).
        e.nextThink = cgi.client->unclampedTime;
        if (e.hz > 0.f) {
          const float interval = 1000.f / e.hz;
          e.nextThink += interval * Randomf();
        }

        $(cg_entities, add, &e);
      }
    }
  }
}

/**
 * @brief Frees the client-side entity array and sets the pointer to `NULL`.
 */
void Cg_FreeEntities(void) {

  if (cg_entities) {
    release(cg_entities);
    cg_entities = NULL;
  }
}

/**
 * @return The entity bound to the client's view.
 */
ClientEntity *Cg_Self(void) {

  int32_t index = cgi.client->frame.ps.entity;

  if (cgi.client->frame.ps.stats[STAT_CHASE]) {
    index = cgi.client->frame.ps.stats[STAT_CHASE];
  }

  return cgi.client->entities + index;
}

/**
 * @brief The player bounding box under the movement parameters the server sent.
 */
Box3 Cg_PlayerBounds(bool ducked) {
  return Pm_Bounds(&cgi.client->frame.ps.pmState.params, ducked);
}

/**
 * @return True if the entity is ducking, false otherwise.
 */
bool Cg_IsDucking(const ClientEntity *ent) {

  const float height = Box3_Size(ent->current.bounds).z;

  const ClientGameClientInfo *ci = Cg_ClientInfo(ent);

  return (ci->standingCeiling - ci->standingFloor) - height > PM_STOP_EPSILON;
}

/**
 * @brief Adds the entity's current sound, if any, to the sound stage.
 */
static void Cg_EntitySound(ClientEntity *ent) {

  EntityState *s = &ent->current;

  if (s->sound) {
    Cg_AddSample(cgi.stage, (const SoundPlaySample *) &(SoundPlaySample) {
      .sample = cgi.client->sounds[s->sound],
      .origin = s->origin,
      .entity = ent,
      .flags = S_PLAY_LOOP | S_PLAY_FRAME
    });
  }

  s->sound = 0;
}

/**
 * @brief Interpolate the current frame, processing any new events and advancing the simulation.
 */
void Cg_Interpolate(const ClientFrame *frame) {

  cgi.client->entity = Cg_Self();

  for (int32_t i = 0; i < frame->numEntities; i++) {

    const uint32_t snum = (frame->entityState + i) & ENTITY_STATE_MASK;
    EntityState *s = &cgi.client->entityStates[snum];

    ClientEntity *ent = &cgi.client->entities[s->number];

    Cg_EntitySound(ent);
    Cg_EntityEvent(ent);

    // the event is consumed whether or not it was shown, so that parsing the
    // same frame again does not fire it twice
    ent->current.event = s->event = 0;
  }
}

/**
 * @brief The tail of the `Cg_AddEntity` chain: the entity's trail, and the
 * render entities its model and effects call for.
 */
static void Cg_AddEntity_Common(ClientEntity *ent) {

  Cg_EntityTrail(ent);

  // set the origin and angles so that we know where to add effects
  RenderEntity e = {
    .id = ent,
    .origin = ent->origin,
    .termination = ent->termination,
    .angles = ent->angles,
    .scale = 1.f,
    .bounds = ent->bounds,
    .absBounds = ent->absBounds,
    .effects = ent->current.effects,
    .color = Color32_Vec4(ent->current.color),
  };

  // add effects, augmenting the renderer entity
  Cg_EntityEffects(ent, &e);

  // if we have no model, we're done
  if (ent->current.model1 == 0) {
    if (!(ent->current.effects & EF_WORLD)) {
      return;
    }
  }

  if (ent->current.effects & EF_CLIENT) {

    // add a client entity, with an animated player model
    Cg_AddClientEntity(ent, &e);

    // add our view weapon, if it's our view entity and we're in first-person
    if (ent == Cg_Self() && !cgi.client->thirdPerson) {
      Cg_AddWeapon(ent, &e);
    }

    return;
  }

  // don't draw our own giblet, since the view is inside it
  if (ent == cgi.client->entity && !cgi.client->thirdPerson) {
    e.effects |= EF_NO_DRAW;
  }

  // assign the model
  e.model = cgi.client->models[ent->current.model1];

  // and any frame animations (button state, etc)
  e.frame = ent->current.animation1;

  // add to view list
  cgi.AddEntity(cgi.view, &e);
}

AddEntity Cg_AddEntity = Cg_AddEntity_Common;

/**
 * @brief Iterate all entities in the current frame, adding models, sprites,
 * lights, and anything else associated with them.
 *
 * The correlation of client entities to renderer entities is not 1:1; some
 * client entities have no visible model, and others (e.g. players) require
 * several.
 */
void Cg_AddEntities(const ClientFrame *frame) {

  if (!cg_add_entities->value) {
    
    // add the world model
    cgi.AddEntity(cgi.view, &(const RenderEntity) {
      .model = cgi.WorldModel()->bsp->worldspawn,
      .scale = 1.f,
      .effects = EF_WORLD
    });
    
    return;
  }

  // add server side entities
  for (int32_t i = 0; i < frame->numEntities; i++) {

    const uint32_t snum = (frame->entityState + i) & ENTITY_STATE_MASK;
    const EntityState *s = &cgi.client->entityStates[snum];
    ClientEntity *ent = &cgi.client->entities[s->number];

    Cg_AddEntity(ent);
  }

  // and client side entities too
  ClientGameEntity *e = cg_entities->elements;
  for (uint32_t i = 0; i < cg_entities->count; i++, e++) {

    if (e->nextThink > cgi.client->unclampedTime) {
      continue;
    }

    e->clazz->Think(e);

    if (e->hz) {
      e->nextThink += 1000.f / e->hz + 1000.f * e->drift * Randomf();
    }
  }
}
