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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cg_local.h"

/**
 * @brief Adds weapon bob due to running, walking, crouching, etc.
 */
static void Cg_WeaponBob(const PlayerState *ps, Vec3 *offset, Vec3 *angles) {
  const Vec3 bob = MakeVec3(0.2f, 0.4f, 0.2f);

  *offset = Vec3_Fmaf(*offset, cgView.bob, bob);
  *angles = Vec3_Add(*angles, MakeVec3(0.f, 1.5f * cgView.bob, 0.f));
}

/**
 * @brief Calculates a kick offset and angles based on our player's animation state.
 */
static void Cg_WeaponOffset(ClientEntity *ent, Vec3 *offset, Vec3 *angles) {

  const Vec3 dropRaiseOffset = MakeVec3(-4.f, -4.f, -4.f);
  const Vec3 dropRaiseAngles = MakeVec3(25.f, -35.f, 2.f);

  const Vec3 kickOffset = MakeVec3(-6.f, 0.f, 0.f);
  const Vec3 kickAngles = MakeVec3(-2.f, 0.f, 0.f);

  *offset = Vec3_Zero();
  *angles = Vec3_Zero();

  if (ent->animation1.animation == ANIM_TORSO_DROP) {
    *offset = Vec3_Fmaf(*offset, ent->animation1.fraction, dropRaiseOffset);
    *angles = Vec3_Scale(dropRaiseAngles, ent->animation1.fraction);
  } else if (ent->animation1.animation == ANIM_TORSO_RAISE) {
    *offset = Vec3_Fmaf(*offset, 1.f - ent->animation1.fraction, dropRaiseOffset);
    *angles = Vec3_Scale(dropRaiseAngles, 1.f - ent->animation1.fraction);
  } else if (ent->animation1.animation == ANIM_TORSO_ATTACK1) {
    *offset = Vec3_Fmaf(*offset, 1.f - ent->animation1.fraction, kickOffset);
    *angles = Vec3_Scale(kickAngles, 1.f - ent->animation1.fraction);
  }

  *offset = Vec3_Scale(*offset, cg_bob->value);
  *angles = Vec3_Scale(*angles, cg_bob->value);

  *offset = Vec3_Add(*offset, MakeVec3(cg_drawWeaponX->value, cg_drawWeaponY->value, cg_drawWeaponZ->value));
}

/**
 * @brief Periodically calculates the player's velocity, and interpolates it
 * over a small interval to smooth out rapid changes.
 */
static void Cg_SpeedModulus(const PlayerState *ps, Vec3 *offset) {
  static Vec3 old_speed, new_speed;
  static uint32_t time;

  if (cgi.client->unclampedTime < time) {
    time = 0;

    old_speed = Vec3_Zero();
    new_speed = Vec3_Zero();
  }

  Vec3 speed;

  const uint32_t delta = cgi.client->unclampedTime - time;
  if (delta < 100) {
    const float lerp = delta / 100.f;

    speed.x = old_speed.x + lerp * (new_speed.x - old_speed.x);
    speed.y = old_speed.y + lerp * (new_speed.y - old_speed.y);
    speed.z = old_speed.z + lerp * (new_speed.z - old_speed.z);
  } else {
    old_speed = new_speed;

    new_speed.x = -Clampf(ps->pmState.velocity.x / 200.f, -1.f, 1.f);
    new_speed.y = -Clampf(ps->pmState.velocity.y / 200.f, -1.f, 1.f);
    new_speed.z = -Clampf(ps->pmState.velocity.z / 200.f, -.3f, 1.f);

    speed = old_speed;

    time = cgi.client->unclampedTime;
  }

  if (cg_drawWeaponBob->modified) {
    cg_drawWeaponBob->value = Clampf(cg_drawWeaponBob->value, 0.0, 2.0);
    cg_drawWeaponBob->modified = false;
  }

  *offset = Vec3_Scale(speed, cg_drawWeaponBob->value);
}

/**
 * @brief Adds the first-person weapon model to the view.
 */
void Cg_AddWeapon(ClientEntity *ent, RenderEntity *self) {
  static RenderEntity w;
  Vec3 offset, angles;
  Vec3 velocity;

  const PlayerState *ps = &cgi.client->frame.ps;

  if (!cg_drawWeapon->value) {
    return;
  }

  if (cgi.client->thirdPerson) {
    return;
  }

  if (ps->stats[STAT_HEALTH] <= 0) {
    return; // dead
  }

  if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
    return; // spectating
  }

  if (cgi.client->demoServer && cgState.spectate.detached) {
    return; // the camera has left the recorded player behind, and their weapon with it
  }

  const int16_t tag = ps->stats[STAT_WEAPON] & 0xFF;
  if (tag < WEAPON_FIRST || tag >= WEAPON_LAST) {
    return; // no weapon, e.g. level intermission
  }
  const int16_t active = tag - WEAPON_FIRST;

  memset(&w, 0, sizeof(w));

  w.origin = cgi.view->origin;

  Cg_WeaponOffset(ent, &offset, &angles);
  Cg_WeaponBob(ps, &offset, &angles);
  Cg_SpeedModulus(ps, &velocity);

  w.origin = Vec3_Add(w.origin, velocity);

  w.model = cgWeapons[active].model;

  if (cgWeapons[active].tag < WEAPON_QUAKE_SHOTGUN) {
    switch (cg_hand->integer) {
      case HAND_LEFT:
        offset.y -= 5.f;
        break;
      case HAND_RIGHT:
        offset.y += 5.f;
        break;
      default:
        break;
    }
  }

  w.origin = Vec3_Fmaf(w.origin, offset.z, cgi.view->up);
  w.origin = Vec3_Fmaf(w.origin, offset.y, cgi.view->right);
  w.origin = Vec3_Fmaf(w.origin, offset.x, cgi.view->forward);

  w.angles = Vec3_Add(cgi.view->angles, angles);

  w.effects = EF_WEAPON | EF_NO_SHADOW;

  w.color = MakeVec4(1.0, 1.0, 1.0, 1.0);

  if (cg_drawWeaponAlpha->value < 1.0) {
    w.effects |= EF_BLEND;
    w.color.w = cg_drawWeaponAlpha->value;
  }

  w.effects |= self->effects & EF_SHELL;
  w.shell = self->shell;

  if (self->effects & EF_INVISIBILITY) {
    w.effects |= EF_BLEND | EF_NO_SHADOW;
    w.color = MakeVec4(1.f, 1.f, 1.f, 0.f);
  }

  w.absBounds = Box3_FromCenterSize(cgi.view->origin, MakeVec3(16.f, 16.f, 16.f));

  w.lerp = w.scale = 1.0;

  RenderEntity *weapon = cgi.AddEntity(cgi.view, &w);

  CGameClientInfo *ci = &cgState.clients[cgi.client->frame.ps.client];

  Vec3 weaponOrigin;
  Mat4_Vectors(weapon->matrix, NULL, NULL, NULL, &weaponOrigin);

  const Vec3 cfgMuzzle = weapon->model->mesh ? weapon->model->mesh->config.view.muzzle : Vec3_Zero();
  if (!Vec3_Equal(cfgMuzzle, Vec3_Zero())) {
    ci->weaponMuzzle = Mat4_Transform(weapon->matrix, cfgMuzzle);
  } else {
    ci->weaponMuzzle = weaponOrigin;
  }
}
