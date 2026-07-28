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

/**
 * @brief Returns an HSV color from a hue pointer, substituting a default hue if the pointer is `NULL` or negative.
 */
vec3_t Cg_EffectColor(float *hue, const float default_hue) {

  if (hue) {
    if (*hue < 0.f) {
      *hue = default_hue;
    }
    return ColorHSV(*hue, 1.f, 1.f).vec3;
  } else {
    return ColorHSV(default_hue, 1.f, 1.f).vec3;
  }
}

/**
 * @brief Returns the effect color for the given client, using their team or personal hue.
 */
vec3_t Cg_ClientEffectColor(const int32_t client, float *hue, const float default_hue) {

  // the client index arrives raw off the wire; fall back to the default hue if it
  // is out of range rather than indexing cg_state.clients[] out of bounds
  float client_hue = default_hue;
  if (client >= 0 && client < MAX_CLIENTS) {
    const cg_client_info_t *ci = &cg_state.clients[client];
    client_hue = ci->team ? ci->team->hue : ci->hue;
  }

  const vec3_t color = Cg_EffectColor(&client_hue, default_hue);

  if (hue) {
    *hue = client_hue;
  }

  return color;
}

/**
 * @brief Adds an inactive player indicator sprite above the entity's position.
 */
static void Cg_InactiveEffect(cl_entity_t *ent, const vec3_t org) {

  if (ent == cgi.client->entity && !cgi.client->third_person) {
    return;
  }

  cgi.AddSprite(cgi.view, &(const r_sprite_t) {
    .origin = Vec3_Add(org, Vec3(0.f, 0.f, 50.f)),
    .color = color_white.vec3,
    .media = (r_media_t *) cg_sprite_inactive,
    .size = 32.f,
  });
}

/**
 * @brief Processes the entity's effects mask, augmenting the renderer entity.
 */
void Cg_EntityEffects(cl_entity_t *ent, r_entity_t *e) {

  e->effects = ent->current.effects;

  if (e->effects & EF_ROTATE) {
    const float rotate = cgi.client->unclamped_time;
    e->angles.y = cg_entity_rotate->value * rotate / M_PI;
  }

  if (e->effects & EF_BOB) {
    e->termination = e->origin;
    const float bob = sinf(cgi.client->unclamped_time * 0.005f + ent->current.number);
    e->origin.z += cg_entity_bob->value * bob;
  }

  if (e->effects & EF_INACTIVE) {
    Cg_InactiveEffect(ent, e->origin);
  }

  if (e->effects & EF_RESPAWN) {
    const vec3_t color = Cg_ClientEffectColor(ent->current.client, NULL, 0.167f);
    e->shell = Vec4_Fmaf(e->shell, 0.5f, Vec3_ToVec4(color, 0.f));
    e->shell.w = fmaxf(e->shell.w, 0.333f);
  }

  if (e->effects & EF_QUAD) {
    const float pulse = 4.f + sinf(cgi.client->unclamped_time * 0.006f) * .75f;
    const cg_light_t l = {
      .origin = e->origin,
      .radius = 350.f,
      .color = Vec3(.2f, .4f, 1.f),
      .intensity = pulse,
      .source = ent,
    };

    Cg_AddLight(&l);

    e->shell = Vec4_Fmaf(e->shell, 1.f, Vec3_ToVec4(l.color, 0.f));
    e->shell.w = fmaxf(e->shell.w, 0.666f);
  }

  if (e->effects & EF_INVULNERABILITY) {
    const float pulse = 4.f + sinf(cgi.client->unclamped_time * 0.006f) * .75f;
    const cg_light_t l = {
      .origin = e->origin,
      .radius = 350.f,
      .color = Vec3(1.f, 0.f, 0.f),
      .intensity = pulse,
      .source = ent,
    };

    Cg_AddLight(&l);

    e->shell = Vec4_Fmaf(e->shell, 1.f, Vec3_ToVec4(l.color, 0.f));
    e->shell.w = fmaxf(e->shell.w, 0.666f);
  }

  if (e->effects & EF_CTF_MASK) {

    for (g_team_id_t team = TEAM_RED; team < MAX_TEAMS; team++) {
      if (e->effects & (EF_CTF_RED << team)) {
        const vec3_t color = Cg_EffectColor(&cg_state.teams[team].hue, 0.f);
        const float pulse = 2.5f + sinf(cgi.client->unclamped_time * 0.005f) * .5f;

        const cg_light_t l = {
          .origin = e->origin,
          .radius = 250.0,
          .color = color,
          .intensity = pulse,
          .source = ent,
        };

        Cg_AddLight(&l);

        e->shell = Vec4_Fmaf(e->shell, 1.f, Vec3_ToVec4(l.color, 0.f));
        e->shell.w = fmaxf(e->shell.w, 0.666f);
      }
    }
  }

  const vec3_t shell_rgb = Vec3(e->shell.x, e->shell.y, e->shell.z);
  if (Vec3_Length(shell_rgb) > 0.f) {
    e->shell = Vec3_ToVec4(Vec3_Normalize(shell_rgb), e->shell.w);
    e->effects |= EF_SHELL;
  }

  e->color = Vec4_One();

  if (e->effects & EF_INVISIBILITY) {
    e->shell = Vec4(1.f, 1.f, 1.f, .125f);
    e->effects |= EF_SHELL | EF_BLEND | EF_NO_SHADOW;
    e->color = Vec4(1.f, 1.f, 1.f, 0.f);
  }

  if (ent->current.trail == TRAIL_QUAKE_NAIL) {
    e->effects |= EF_NO_SHADOW;
  }

  if (e->effects & EF_DESPAWN) {

    if (!(ent->prev.effects & EF_DESPAWN)) {
      ent->timestamp = cgi.client->unclamped_time;
    }

    e->effects |= (EF_BLEND | EF_NO_SHADOW);

    const float fade = 1.f - (cgi.client->unclamped_time - ent->timestamp) / 3000.f;
    const float clamped_fade = Clampf01(fade);
    e->color = Vec4_Scale(e->color, clamped_fade);
  }

  if (e->effects & EF_LIGHT) {
    Cg_AddLight(&(const cg_light_t) {
      .origin = e->origin,
      .radius = ent->current.termination.x,
      .color = Color32_Color(ent->current.color).vec3,
      .intensity = 1.f,
      .source = ent
    });
  }

  if (e->effects & EF_LIGHT_PULSE) {
    const float pulse = .25f + .75f * (1.f + sinf(cgi.client->unclamped_time * .003f)) * .5f;
    Cg_AddLight(&(const cg_light_t) {
      .origin = Vec3_Fmaf(e->origin, 32.f, Vec3_Up()),
      .radius = ent->current.termination.x * pulse,
      .color = Color32_Color(ent->current.color).vec3,
      .intensity = 1.f,
    });
  }

  if (e->effects & EF_TEAM_TINT) {
    assert(ent->current.animation1 < MAX_TEAMS);

    const cg_team_info_t *team = cg_state.teams + ent->current.animation1;
    e->tints[0] = Vec4(team->color.r, team->color.g, team->color.b, 1.f);

    for (int32_t i = 1; i < 3; i++) {
      e->tints[i] = e->tints[0];
    }
  }
}
