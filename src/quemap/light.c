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

#include "light.h"
#include "points.h"
#include "qlight.h"
#include <Objectively/Vector.h>

Vector *lights = NULL;

/**
 * @brief Allocates and returns a new light structure.
 */
static Light *AllocLight(void) {
  Light *light = Mem_TagMalloc(sizeof(Light), (MemTag) MEM_TAG_LIGHT);
  light->targetEntity = -1;
  return light;
}

/**
 * @brief Frees a single light structure.
 */
static void FreeLight(Light *light) {
  Mem_Free(light);
}

/**
 * @brief Finds the `teamMaster` light entity for the given team.
 */
static const CmEntity *FindTeamMaster(const char *team) {

  if (!team) {
    return NULL;
  }

  CmEntity **e = Cm_Bsp()->entities;
  for (int32_t i = 0; i < Cm_Bsp()->numEntities; i++, e++) {
    const char *classname = Cm_EntityValue(*e, "classname")->string;
    if (!q_strcmp(classname, "light")) {
      const char *entTeam = Cm_EntityValue(*e, "team")->nullableString;
      if (entTeam && !q_strcmp(entTeam, team)) {
        if (Cm_EntityValue(*e, "team_master")->parsed) {
          return *e;
        }
      }
    }
  }

  return NULL;
}

/**
 * @brief Parses a light entity and returns a populated `Light`, or `NULL` if the entity is not a light.
 */
static Light *LightForEntity(const CmEntity *entity) {

  const char *classname = Cm_EntityValue(entity, "classname")->string;
  if (!q_strcmp(classname, "light")) {

    Light *light = AllocLight();

    light->entity = Cm_EntityNumber(entity);
    light->origin = Cm_EntityValue(entity, "origin")->vec3;
    light->radius = Cm_EntityValue(entity, "radius")->value;
    light->color = Cm_EntityValue(entity, "color")->vec3;
    light->intensity = Cm_EntityValue(entity, "intensity")->value;
    q_strlcpy(light->style, Cm_EntityValue(entity, "style")->string, sizeof(light->style));

    const float drift = Cm_EntityValue(entity, "drift")->value;

    const CmEntity *master = FindTeamMaster(Cm_EntityValue(entity, "team")->nullableString);
    if (master) {
      light->radius = light->radius ?: Cm_EntityValue(master, "radius")->value;

      if (Vec3_Equal(Vec3_Zero(), light->color)) {
        light->color = Cm_EntityValue(master, "color")->vec3;
      }

      light->intensity = light->intensity ?: Cm_EntityValue(master, "intensity")->value;

      if (!*light->style) {
        q_strlcpy(light->style, Cm_EntityValue(master, "style")->string, sizeof(light->style));
      }

      if (!light->drift) {
        light->drift = Cm_EntityValue(master, "drift")->value;
      }
    }

    light->radius = light->radius ?: LIGHT_RADIUS;

    if (Vec3_Equal(Vec3_Zero(), light->color)) {
      light->color = LIGHT_COLOR;
    }

    light->intensity = light->intensity ?: LIGHT_INTENSITY;

    // Compute per-light phase from origin hash, scaled by drift.
    // This gives each compiled light instance a unique stable offset.
    const float effectiveDrift = drift ?: light->drift;
    if (effectiveDrift > 0.f) {
      const float h = fabsf(sinf(light->origin.x * 127.1f +
                                 light->origin.y * 311.7f +
                                 light->origin.z *  74.7f));
      light->drift = effectiveDrift * fmodf(h, 1.f);
    }

    light->bounds = Box3_FromCenterRadius(light->origin, light->radius);
    light->visibleBounds = Box3_Null();

    // Entity-attached lights target an inline model entity and move with it at runtime.
    // Resolve the target entity number now so the BSP carries the reference.
    const char *target = Cm_EntityValue(entity, "target")->nullableString;
    if (target) {
      const CmBsp *bsp = Cm_Bsp();
      for (int32_t i = 0; i < bsp->numEntities; i++) {
        const char *targetname = Cm_EntityValue(bsp->entities[i], "targetname")->nullableString;
        if (!q_strcmp(targetname, target)) {
          light->targetEntity = i;
          break;
        }
      }

      if (light->targetEntity == -1) {
        Com_Warn("Entity light @ %s: target \"%s\" not found\n", vtos(light->origin), target);
      }
    }

    return light;
  } else {
    return NULL;
  }
}

/**
 * @brief Frees all lights and releases the lights array.
 */
void FreeLights(void) {

  if (!lights) {
    return;
  }

  for (size_t i = 0; i < lights->count; i++) {
    FreeLight(VectorValue(lights, Light *, i));
  }

  lights = release(lights);
}

/**
 * @brief Parses all light entities from the BSP and populates the lights array.
 */
void BuildLights(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  Progress("Building lights", 0);

  lights = lights ?: $(alloc(Vector), initWithSize, sizeof(Light *));

  CmEntity **entity = Cm_Bsp()->entities;
  for (int32_t i = 0; i < Cm_Bsp()->numEntities; i++, entity++) {
    Light *light = LightForEntity(*entity);
    if (light) {
      $(lights, add, &light);
    }
    Progress("Building lights", i * 100.f / Cm_Bsp()->numEntities);
  }

  Com_Print("\r%-24s [100%%] %d ms\n", "Building lights", (uint32_t) SDL_GetTicks() - start);

  Com_Verbose("Lighting for %zu lights\n", lights->count);
}

/**
 * @brief Writes the lights array to the BSP lights lump.
 */
void EmitLights(void) {

  if (!lights) {
    return;
  }

  const uint32_t start = (uint32_t) SDL_GetTicks();

  if ((int32_t) lights->count >= MAX_BSP_LIGHTS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTS\n");
  }

  Bsp_AllocLump(&bspFile, BSP_LUMP_ELEMENTS, MAX_BSP_ELEMENTS);
  Bsp_AllocLump(&bspFile, BSP_LUMP_DRAW_ELEMENTS, MAX_BSP_DRAW_ELEMENTS);
  Bsp_AllocLump(&bspFile, BSP_LUMP_LIGHTS, lights->count);

  BspLight *out = bspFile.lights;
  for (size_t i = 0; i < lights->count; i++) {

    Light *light = VectorValue(lights, Light *, i);

    if (light->targetEntity != -1) {
      // These will use the dynamic lighting code path at runtime and can not use precomputed
      // bounds or voxelization, because they can move; simply emit them to the BSP.
    } else {
      if (Box3_IsNull(light->visibleBounds)) {
        Com_Warn("light @ %s has no visible bounds; is it inside of CONTENTS_SOLID?\n", vtos(light->origin));
        continue;
      }
    }

    light->out = out;

    out->entity = light->entity;
    out->origin = light->origin;
    out->radius = light->radius;
    out->color = light->color;
    out->intensity = light->intensity;
    out->bounds = light->visibleBounds;
    out->targetEntity = light->targetEntity;
    q_strlcpy(out->style, light->style, sizeof(out->style));
    out->drift = light->drift;

    if (light->targetEntity == -1) {
      out->firstDrawElements = bspFile.numDrawElements;

      if (bspFile.numDrawElements == MAX_BSP_DRAW_ELEMENTS) {
        Com_Error(ERROR_FATAL, "MAX_BSP_DRAW_ELEMENTS\n");
      }

      // Opaque faces are lumped into a single draw elements, with a sentinel
      // material of -1, since the shadow pass does not sample any texture
      // for them. Alpha-tested faces (foliage, fences, grates) are grouped
      // by material below, so their diffuse texture can be sampled and
      // discarded per-pixel at draw time.
      BspDrawElements *opaque = bspFile.drawElements + bspFile.numDrawElements;
      opaque->material = -1;
      opaque->bounds = Box3_Null();
      opaque->firstElement = bspFile.numElements;

      Vector *alphaTestFaces = $(alloc(Vector), initWithSize, sizeof(BspFace *));

      const BspModel *worldspawn = bspFile.models;
      const BspFace *face = &bspFile.faces[worldspawn->firstFace];
      for (int32_t j = 0; j < worldspawn->numFaces; j++, face++) {

        if (!Box3_Intersects(face->bounds, out->bounds)) {
          continue;
        }

        int32_t surface;
        int32_t contents;
        if (face->brushSide >= 0) {
          const BspBrushSide *side = &bspFile.brushSides[face->brushSide];
          surface = side->surface;
          contents = side->contents;
        } else {
          const BspPatch *patch = &bspFile.patches[face->patch];
          surface = patch->surface;
          contents = patch->contents;
        }

        if (surface & SURF_ALPHA_TEST) {
          $(alphaTestFaces, add, &face);
          continue;
        }

        if (contents & CONTENTS_MIST) {
          continue;
        }

        if (surface & SURF_MASK_NO_DRAW_ELEMENTS) {
          continue;
        }

        if (surface & SURF_LIQUID) {
          continue;
        }

        if (surface & (SURF_MASK_BLEND | SURF_MATERIAL)) {
          continue;
        }

        if (bspFile.numElements + face->numElements >= MAX_BSP_ELEMENTS) {
          Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
        }

        memcpy(bspFile.elements + bspFile.numElements, bspFile.elements + face->firstElement, sizeof(int32_t) * face->numElements);

        bspFile.numElements += face->numElements;

        opaque->numElements += face->numElements;
        opaque->bounds = Box3_Union(opaque->bounds, face->bounds);
      }

      if (opaque->numElements) {
        bspFile.numDrawElements++;
      }

      if (alphaTestFaces->count) {
        EmitDrawElements(alphaTestFaces);
      }

      release(alphaTestFaces);

      out->numDrawElements = bspFile.numDrawElements - out->firstDrawElements;
    }

    out++;

    Progress("Emitting lights", 100.f * i / lights->count);
  }

  bspFile.numLights = (int32_t) (ptrdiff_t) (out - bspFile.lights);

  Com_Print("\r%-24s [100%%] %d ms\n", "Emitting lights", (uint32_t) SDL_GetTicks() - start);
}
