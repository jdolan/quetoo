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

// weather emitters are bound to downward-facing sky surfaces
typedef struct cg_weather_emit_s {
  const r_bsp_face_t *face;
  int32_t num_origins; // the number of origins
  vec4_t *origins; // the origins for particle spawns
  struct cg_weather_emit_s *next;
} cg_weather_emit_t;

typedef struct {
  cg_weather_emit_t *emits;
  uint32_t time;
} cg_weather_state_t;

static cg_weather_state_t cg_weather_state;

/**
 * @brief Parses the weather worldspawn key, e.g. "rain," "snow."
 */
int32_t Cg_ParseWeather(const char *string) {

  int32_t weather = WEATHER_NONE;

  if (strstr(string, "rain")) {
    weather |= WEATHER_RAIN;
  }

  if (strstr(string, "snow")) {
    weather |= WEATHER_SNOW;
  }

  if (strstr(string, "ash")) {
      weather |= WEATHER_ASH;
  }

  return weather;
}

/**
 * @brief Creates an emitter for the given surface. The number of origins for the
 * emitter depends on the area of the surface.
 */
static void Cg_LoadWeather_(const r_bsp_face_t *face) {

  cg_weather_emit_t *e = cgi.Malloc(sizeof(cg_weather_emit_t), MEM_TAG_CGAME_LEVEL);

  // resolve the leaf for the point just in front of the surface

  vec3_t center = Box3_Center(face->bounds);
  center = Vec3_Add(center, face->brush_side->plane->cm->normal);

  e->face = face;

  // resolve the number of origins based on surface area
  e->num_origins = Vec3_Length(Box3_Size(face->bounds)) / 16.f;
  e->num_origins = Clampf(e->num_origins, 1, 128);

  e->origins = cgi.Malloc(sizeof(vec4_t) * e->num_origins, MEM_TAG_CGAME_LEVEL);

  // resolve the origins and their end positions

  int32_t i = 0, j = 0;
  while (i < e->num_origins) {

    if (j++ == INT16_MAX) {
      Cg_Debug("Failed to resolve weather effects @%s. Does your map leak?\n", vtos(center));
      break;
    }
    
    vec4_t *origin = e->origins + i;

    // randomize the origin over the surface

    const vec3_t org = Vec3_Add(Box3_RandomPoint(face->bounds), face->plane->cm->normal);

    vec3_t end = org;
    end.z -= 11586.f;//FIXME: is undefined. MAX_WORLD_DIST not exported??

    const cm_trace_t tr = cgi.Trace(org, end, Box3_Zero(), NULL, CONTENTS_MASK_CLIP_PROJECTILE | CONTENTS_MASK_LIQUID | CONTENTS_MASK_SOLID);
    if (tr.fraction == 1.f) {
        continue;
    }

    *origin = Vec3_ToVec4(org, tr.end.z);

    i++;
  }

  // push on to the linked list
  e->next = cg_weather_state.emits;
  cg_weather_state.emits = e;

  Cg_Debug("%s: %d origins\n", vtos(center), e->num_origins);
}

/**
 * @brief Iterates the world surfaces, generating weather emitters from sky brushes.
 * Valid weather origins and z-depths are resolved and cached.
 */
static void Cg_LoadWeather(void) {
  int32_t i, j;

  cg_weather_state.emits = NULL;
  cg_weather_state.time = 0;

  const char *weather = cgi.EntityValue(cgi.Worldspawn(), "weather")->string;
  cg_state.weather = Cg_ParseWeather(weather);

  if (!cg_state.weather) {
    return;
  }

  const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;
  const r_bsp_face_t *face = bsp->faces;

  // iterate the world surfaces, testing sky surfaces
  for (i = j = 0; i < bsp->num_faces; i++, face++) {

    const r_bsp_brush_side_t *side = face->brush_side;

    // for downward facing sky brushes, create an emitter
    if ((side->surface & SURF_SKY) && side->plane->cm->normal.z < -0.1f) {
      Cg_LoadWeather_(face);
      j++;
    }
  }

  Cg_Debug("%d emits\n", j);
}

/**
 * @brief Loads all resources required by client-side effects such as weather.
 */
void Cg_LoadEffects(void) {
  Cg_LoadWeather();
}

/**
 * @brief Adds weather sprites for the specified emitter. The number of sprites
 * added is dependent on the size of the surface associated with the emitter.
 */
static void Cg_AddWeather_(const cg_weather_emit_t *e) {

  for (int32_t i = 0; i < e->num_origins; i++) {

    const vec4_t origin = *(e->origins + i);

    const vec3_t org_xy = Vec3(origin.x, origin.y, cgi.view->origin.z);

    if (Vec3_DistanceSquared(org_xy, cgi.view->origin) > 1024.f * 1024.f) {
      continue;
    }

    const float z = Maxf(cgi.view->origin.z, origin.w);
    const vec3_t org = Vec3(origin.x, origin.y, RandomRangef(z, origin.z));

    cg_sprite_t *s = NULL;

    if (cg_state.weather & WEATHER_RAIN) {
      s = Cg_AddSprite(&(cg_sprite_t) {
        .origin = org,
        .atlas_image = cg_sprite_rain,
        .color = Vec3(.3f, .3f, .4f),
        .size = 32.f,
        .velocity = Vec3_Subtract(Vec3_RandomRange(-2.f, 2.f), Vec3(0.f, 0.f, 800.f)),
        .axis = SPRITE_AXIS_X | SPRITE_AXIS_Y,
      });

      if (Randomf() > .8f) {
        Cg_AddSprite(&(cg_sprite_t) {
          .atlas_image = cg_sprite_water_ring,
          .lifetime = 300.f,
          .origin = Vec3(org_xy.x, org_xy.y, origin.w),
          .size = 4.f,
          .size_velocity = 4.f * 6.f,
          .rotation = RandomRadian(),
          .dir = Vec3_Up(),
          .color = Vec3(.8f, .8f, .8f),
          .lighting = 1.f
        });
      }

    } else if (cg_state.weather & WEATHER_SNOW) {
      s = Cg_AddSprite(&(cg_sprite_t) {
        .origin = org,
        .atlas_image = cg_sprite_snow,
        .color = Vec3(1.f, 1.f, 1.f),
        .size = 4.f,
        .velocity = Vec3_Subtract(Vec3_RandomRange(-12.f, 12.f), Vec3(0.f, 0.f, 120.f)),
        .acceleration = Vec3_RandomRange(-12.f, 12.f),
      });
    }
    else if (cg_state.weather & WEATHER_ASH) {
        float color = RandomRangef(0.25f, 0.75f);
        float size = RandomRangef(1.f, 3.f);
        s = Cg_AddSprite(&(cg_sprite_t) {
            .origin = org,
                .atlas_image = cg_sprite_ash,
                .color = Vec3(color, color, color),
                .size = size,
                .velocity = Vec3_Subtract(Vec3_RandomRange(-12.f, 12.f), Vec3(0.f, 0.f, 25.f)),
                .acceleration = Vec3_RandomRange(-12.f, 12.f),
                .rotation = RandomRadian()
        });
    }

    if (s) {
      s->lifetime = 1000 * (org.z - origin.w) / fabsf(s->velocity.z);
    }
  }
}

/**
 * @brief Adds particles and issues ambient loop sounds for weather effects.
 */
static void Cg_AddWeather(void) {

  if (!cg_add_weather->value) {
    return;
  }

  if (cg_state.weather == WEATHER_NONE) {
    return;
  }

  const s_sample_t *sample = NULL;

  if (cg_state.weather & WEATHER_RAIN) {
    sample = cg_sample_rain;
  } else if (cg_state.weather & WEATHER_SNOW) {
    sample = cg_sample_snow;
  } else if (cg_state.weather & WEATHER_ASH) {
    sample = cg_sample_ash;
  }

  if (sample) {
    Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
      .sample = sample,
      .flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME,
      .entity = Cg_Self()
    });
  }

  if (cg_weather_state.time > cgi.client->unclamped_time) {
    return;
  }

  const cg_weather_emit_t *e = cg_weather_state.emits;

  while (e) {
    Cg_AddWeather_(e);
    e = e->next;
  }

  if (cg_state.weather & WEATHER_RAIN) {
    cg_weather_state.time = cgi.client->unclamped_time + RandomRangeu(100, 300);
  } else if (cg_state.weather & WEATHER_SNOW) {
    cg_weather_state.time = cgi.client->unclamped_time + RandomRangeu(300, 600);
  }
}

/**
 * @brief Adds an ambient loop sound when the player's view is underwater.
 */
static void Cg_AddUnderwater(void) {

  if (cgi.view->contents & CONTENTS_MASK_LIQUID) {
    Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
      .sample = cg_sample_underwater,
      .flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME,
      .entity = Cg_Self()
    });
  }
}

/**
 * @brief
 */
void Cg_AddEffects(void) {

  Cg_AddWeather();

  Cg_AddUnderwater();
}
