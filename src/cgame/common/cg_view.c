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

cg_view_t cg_view;

#define CG_FOV_REFERENCE_ASPECT (16.f / 9.f)

/**
 * @brief Computes the half-angle horizontal and vertical FOV, in degrees, for the
 * given reference FOV (horizontal, at @c CG_FOV_REFERENCE_ASPECT) and viewport size.
 */
static vec2_t Cg_Fov(float fov_degrees, float width, float height) {

  const float fov_half = Radians(fov_degrees / 2.f);
  const float aspect = width / height;

  if (aspect >= CG_FOV_REFERENCE_ASPECT) {
    const float fov_y = atanf(tanf(fov_half) / CG_FOV_REFERENCE_ASPECT);
    const float fov_x = atanf(tanf(fov_y) * aspect);
    return Vec2(Degrees(fov_x), Degrees(fov_y));
  } else {
    const float fov_y = atanf(tanf(fov_half) / aspect);
    return Vec2(Degrees(fov_half), Degrees(fov_y));
  }
}

/**
 * @brief Recovers the reference FOV value (horizontal FOV at
 * @c CG_FOV_REFERENCE_ASPECT) that produced the given actual half-angle FOV at the
 * given viewport size. The inverse of @c Cg_Fov, used to reconstruct the FOV we are
 * interpolating away from when @c cg_fov changes mid-transition.
 */
static float Cg_FovInverse(const vec2_t fov, float width, float height) {

  const float aspect = width / height;

  if (aspect >= CG_FOV_REFERENCE_ASPECT) {
    const float fov_half = atanf(tanf(Radians(fov.y)) * CG_FOV_REFERENCE_ASPECT);
    return 2.f * Degrees(fov_half);
  } else {
    return 2.f * fov.x;
  }
}

/**
 * @brief Update the field of view, which affects the view port as well as the culling
 * frustum.
 */
static void Cg_UpdateFov(void) {

  cg_fov->value = Clampf(cg_fov->value, 10.f, 160.f);
  cg_fov_interpolate->value = Clampf(cg_fov_interpolate->value, 0.f, 10.f);

  const float width = cgi.view->viewport.z;
  const float height = cgi.view->viewport.w;

  float fov = cg_fov->value;

  if (cg_fov_interpolate->value && cgi.view->fov.x && cgi.view->fov.y) {
    static float prev, next;
    static uint32_t time;

    if (next && next != cg_fov->value) {
      time = 0;
    }

    if (time == 0) {
      prev = Cg_FovInverse(cgi.view->fov, width, height);
      next = cg_fov->value;
      time = cgi.client->unclamped_time;
    }

    const float frac = (cgi.client->unclamped_time - time) / (cg_fov_interpolate->value * 100.f);
    if (frac >= 1.f) {
      time = 0;
      fov = next;
      cg_fov->modified = false;
    } else {
      fov = prev + frac * (next - prev);
    }
  } else {
    cg_fov->modified = false;
  }

  cgi.view->fov = Cg_Fov(fov, width, height);
}

/**
 * @brief Returns true if the camera has a subject to frame: the recorded player during demo
 * playback, or a chase target while spectating a live game. Without one there is nothing to
 * frame, and the camera is flying free.
 */
bool Cg_CameraSubject(const player_state_t *ps) {

  if (cgi.client->demo_server) {
    return !cg_state.spectate.detached;
  }

  return ps->stats[STAT_CHASE];
}

/**
 * @brief Returns true if the third-person offset should be driven by the viewer's mouse and
 * `+forward`/`+back` (`cg_state.follow`) rather than the static `cg_third_person_*` cvars.
 * @details That input is free to take in exactly these states: a chasing spectator's aim is
 * never read by the game module (`G_ClientChaseThink` overwrites their view entirely), and demo
 * playback sends no commands to anything. A player forcing `cg_third_person` while actually
 * playing is excluded, since their mouse and movement keys are busy.
 */
bool Cg_FollowEligible(const player_state_t *ps) {
  return cg_state.camera_mode == CAMERA_FOLLOW && Cg_CameraSubject(ps);
}

/**
 * @brief Publishes the mode to `cg_camera_mode` so the console reflects what the camera is
 * doing. This is a statement, not a request: it clears `modified` so that what is written here
 * does not come back as one.
 */
static void Cg_PublishCameraMode(void) {

  if (cg_camera_mode->integer != (int32_t) cg_state.camera_mode) {
    cgi.SetCvarValue(cg_camera_mode->name, cg_state.camera_mode);
  }

  cg_camera_mode->modified = false;
}

/**
 * @brief Takes any mode the player has set on `cg_camera_mode`.
 * @details Nothing has to be reconciled with the server here: the mode says only how a subject
 * is framed, and whether there is a subject at all is asked separately, so changing the camera
 * can never cost a chase target.
 */
static void Cg_UpdateCameraMode(void) {

  if (cg_camera_mode->modified) {
    cg_state.camera_mode = Mini(Maxi(cg_camera_mode->integer, 0), CAMERA_MODE_TOTAL - 1);
  }

  Cg_PublishCameraMode();
}

/**
 * @brief Prints the camera controls once per connection, the first time the viewer has a camera
 * of their own to steer - spectating a live game, or playing a demo back. The transport controls
 * a demo also gets are printed by `Cl_ParseServerData`, which knows a demo is starting.
 * @remarks The keys named are the shipped defaults, which is all this can honestly claim: they
 * are bindings, and a player may have moved them.
 */
static void Cg_PrintControls(const player_state_t *ps) {

  if (cg_state.printed_controls) {
    return;
  }

  const bool demo = cgi.client->demo_server;

  if (!demo && !ps->stats[STAT_SPECTATOR]) {
    return;
  }

  cg_state.printed_controls = true;

  cgi.Print("^3Camera controls:^7\n");
  cgi.Print("  Cycle camera:  ^2MOUSE2^7\n");
  cgi.Print("  Watch/free:    ^2MOUSE1^7\n");

  if (!demo) {
    cgi.Print("  Change target: ^2WHEEL^7\n");
  }

  cgi.Print("  Aim camera:    ^2MOUSE^7\n");
  cgi.Print("  Camera dist:   ^2W / S^7\n");
}

/**
 * @brief Console command: advances to the next camera mode, wrapping around.
 * @details Cycling is a command rather than a `toggle` of `cg_camera_mode` because `toggle`
 * here is strictly boolean; setting the cvar outright still works, and lands in the same place.
 */
void Cg_CameraModeCycle_f(void) {

  cg_state.camera_mode = (cg_state.camera_mode + 1) % CAMERA_MODE_TOTAL;

  Cg_PublishCameraMode();
}

/**
 * @brief Update the third person offset, if any. This is used as a client-side
 * option, as the default chase camera view, and as the follow camera.
 */
static void Cg_UpdateThirdPerson(const player_state_t *ps) {
  vec3_t forward, right, up, origin, point;

  const box3_t bounds = Box3f(32.f, 32.f, 32.f);

  if (ps->pm_state.flags & PMF_DEATH_CAM) {
    // the game has already resolved the camera into the view offset
    cgi.client->third_person = true;
    return;
  }

  const bool follow = Cg_FollowEligible(ps);

  if (follow && !cg_state.follow.following) {
    // entering follow: seed from where the view already is, so the camera takes over from the
    // subject's own orientation without a jump
    cg_state.follow.yaw = cgi.view->angles.y + cg_third_person_yaw->value;
    cg_state.follow.pitch = cgi.view->angles.x + cg_third_person_pitch->value;
    cg_state.follow.distance = -cg_third_person_x->value;
  }
  cg_state.follow.following = follow;

  const bool third_person = cg_state.camera_mode == CAMERA_THIRD_PERSON && Cg_CameraSubject(ps);

  if (cg_third_person->value && Cg_Self()->current.model1) {
    cgi.client->third_person = true;
  } else if (follow || third_person) {
    cgi.client->third_person = true;
  } else {
    cgi.client->third_person = false;
    return;
  }

  vec3_t offset;
  vec3_t angles;

  if (follow) {
    offset = Vec3(-cg_state.follow.distance, cg_third_person_y->value, cg_third_person_z->value);

    // absolute, not relative to the subject: the camera holds its place in the world while the
    // player being watched turns, which is what makes it usable for reviewing a fight
    angles = Vec3_ClampEuler(Vec3(cg_state.follow.pitch, cg_state.follow.yaw, 0.f));
  } else {
    offset = Vec3(
      cg_third_person_x->value,
      cg_third_person_y->value,
      cg_third_person_z->value
    );

    angles = Vec3_ClampEuler(Vec3(
      cgi.view->angles.x + cg_third_person_pitch->value,
      cgi.view->angles.y + cg_third_person_yaw->value,
      cgi.view->angles.z
    ));
  }

  const float yaw = angles.y;

  Vec3_Vectors(angles, &forward, &right, &up);

  point = Vec3_Fmaf(cgi.view->origin, 512.f, forward);

  origin = Vec3_Fmaf(cgi.view->origin, offset.z, up);
  origin = Vec3_Fmaf(origin, offset.y, right);
  origin = Vec3_Fmaf(origin, offset.x, forward);

  const cm_trace_t tr = cgi.Trace(cgi.view->origin, origin, bounds, NULL, CONTENTS_MASK_CLIP_PLAYER);
  cgi.view->origin = tr.end;

  point = Vec3_Subtract(point, cgi.view->origin);
  cgi.view->angles = Vec3_Euler(Vec3_Normalize(point));
  cgi.view->angles.y = yaw;

  Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
}

/**
 * @brief Periodically calculates the player's horizontal speed, and interpolates it
 * over a small interval to smooth out rapid changes in velocity.
 */
static float Cg_BobSpeedModulus(const player_state_t *ps) {
  static float old_speed, new_speed;
  static uint32_t time;

  if (cgi.client->unclamped_time < time) {
    time = 0;
    old_speed = new_speed = 0.f;
  }

  float speed;

  const uint32_t delta = cgi.client->unclamped_time - time;
  if (delta < 200) {
    const float lerp = delta / (float) 200;
    speed = old_speed + lerp * (new_speed - old_speed);
  } else {
    const bool ducked = ps->pm_state.flags & PMF_DUCKED;
    const float max_speed = ducked ? PM_SPEED_DUCKED : PM_SPEED_AIR;

    vec3_t velocity = ps->pm_state.velocity;
    velocity.z = 0.0;

    old_speed = new_speed;
    new_speed = Vec3_Length(velocity) / max_speed;
    new_speed = Clampf01(new_speed);
    speed = old_speed;

    time = cgi.client->unclamped_time;
  }

  return 0.66f + speed;
}

/**
 * @brief Calculate the view bob. This is done using a running time counter and a
 * simple sin function. The player's speed, as well as whether or not they
 * are on the ground, determine the bob frequency and amplitude.
 */
static void Cg_UpdateBob(const player_state_t *ps) {
  static uint32_t time;
  static float bob;

  if (!cg_bob->value) {
    return;
  }

  if (cgi.client->third_person) {
    return;
  }

  if (cgi.client->demo_server && cg_state.spectate.detached) {
    return; // a free camera does not walk, least of all to the gait of the player it left
  }

  if (ps->pm_state.type >= PM_SPECTATOR) {

    // if we're frozen and not chasing, don't bob
    if (!ps->stats[STAT_CHASE]) {
      return;
    }
  }

  if (cg_bob->modified) {
    cgi.SetCvarValue(cg_bob->name, Clampf(cg_bob->value, 0.f, 2.f));
    cg_bob->modified = false;
  }

  if (cgi.client->unclamped_time < time) {
    bob = time = 0;
  }

  const float mod = Cg_BobSpeedModulus(ps);

  // then calculate how much bob to add this frame
  float frame_bob = Clampf(cgi.client->unclamped_time - time, 1u, 1000u) * mod;

  if (!(ps->pm_state.flags & PMF_ON_GROUND)) {
    frame_bob *= 0.25f;
  }

  bob += frame_bob;
  time = cgi.client->unclamped_time;

  cg_view.bob = sinf(0.0066f * bob) * mod * mod;
  cg_view.bob *= cg_bob->value; // scale via cvar too

  cgi.view->origin = Vec3_Fmaf(cgi.view->origin, -cg_view.bob, cgi.view->forward);
  cgi.view->origin = Vec3_Fmaf(cgi.view->origin,  cg_view.bob, cgi.view->right);
  cgi.view->origin = Vec3_Fmaf(cgi.view->origin,  cg_view.bob, cgi.view->up);
}

/**
 * @brief Resolves the view origin for the pending frame.
 * @param ps0 The player state to interpolate from.
 * @param ps1 The player state to interpolate to.
 */
static void Cg_UpdateOrigin(const player_state_t *ps0, const player_state_t *ps1) {

  if (cgi.client->demo_server && cg_state.spectate.detached) {
    cgi.view->origin = cg_state.spectate.state.origin;
    return;
  }

  if (Cg_UsePrediction()) {
    cl_predicted_state_t *pr = &cgi.client->predicted_state;
    cgi.view->origin = Vec3_Add(pr->view.origin, pr->view.offset);

    const vec3_t error = Vec3_Scale(pr->error, 1.f - cgi.client->lerp);
    cgi.view->origin = Vec3_Add(cgi.view->origin, error);

    cgi.view->origin.z -= pr->view.step_offset;
  } else {
    vec3_t ps0_org = Vec3_Add(ps0->pm_state.origin, ps0->pm_state.view_offset);
    ps0_org.z -= ps0->pm_state.step_offset;

    vec3_t ps1_org = Vec3_Add(ps1->pm_state.origin, ps1->pm_state.view_offset);
    ps1_org.z -= ps1->pm_state.step_offset;

    cgi.view->origin = Vec3_Mix(ps0_org, ps1_org, cgi.client->lerp);
  }
}

/**
 * @brief Resolves the view angles for the pending frame.
 * @param ps0 The player state to interpolate from.
 * @param ps1 The player state to interpolate to.
 */
static void Cg_UpdateAngles(const player_state_t *ps0, const player_state_t *ps1) {
  vec3_t angles, angles0, angles1;

  if (cgi.client->demo_server && cg_state.spectate.detached) {
    cgi.view->angles = cg_state.spectate.state.view_angles;
    Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
    return;
  }

  if (cg_state.snap_angles) {
    // Server requests an immediate snap to the authoritative view angles.
    // Bypass all interpolation and prediction, and also fix up cl.angles so
    // that subsequent input and prediction frames start from the right place.
    cgi.view->angles = cg_state.snap_view_angles;
    cgi.client->angles = cg_state.snap_view_angles;
    Cg_ClearInput();

    Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
    cg_state.snap_angles = false;
    return;
  }

  if (Cg_UsePrediction()) {
    const cl_predicted_state_t *pr = &cgi.client->predicted_state;
    cgi.view->angles = pr->view.angles;
  } else {

    angles0 = ps0->pm_state.view_angles;
    angles1 = ps1->pm_state.view_angles;

    cgi.view->angles = Vec3_MixEuler(angles0, angles1, cgi.client->lerp);
  }

  angles0 = ps0->pm_state.delta_angles;
  angles1 = ps1->pm_state.delta_angles;

  angles = angles1;

  // for small delta angles, such as riding a rotator, interpolate them
  if (!Vec3_Equal(angles0, angles1)) {
    int32_t i;

    for (i = 0; i < 3; i++) {
      const float delta = fabs(angles1.xyz[i] - angles0.xyz[i]);
      if (delta > 5.f && delta < 355.f) {
        break;
      }
    }

    if (i == 3) {
      angles = Vec3_MixEuler(angles0, angles1, cgi.client->lerp);
    }
  }

  cgi.view->angles = Vec3_Add(cgi.view->angles, angles);

  if (ps1->pm_state.type == PM_DEAD) {
    if (!(ps1->pm_state.flags & PMF_DEATH_CAM)) { // the death camera needs its pitch
      cgi.view->angles.x = 0.0;
    }
  } else if (ps1->pm_state.type == PM_FREEZE) {
    cgi.client->angles = cgi.view->angles;
  }

  Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
}

/**
 * @brief Updates the view ambient light level from the worldspawn entity definition.
 */
static void Cg_UpdateAmbient(void) {

  const cm_entity_t *worldspawn = editor->value
  ? cg_editor.entities[0].def
  : cgi.WorldModel()->bsp->cm->entities[0];

  const cm_entity_t *ambient = cgi.EntityValue(worldspawn, "ambient");
  cgi.view->ambient = ambient->value;
}

/**
 * @brief Updates the view origin, angles, and field of view.
 */
void Cg_PrepareView(const cl_frame_t *frame) {

  cgi.view->type = VIEW_MAIN;
  cgi.view->flags = VIEW_FLAG_NONE;

  assert(cg_framebuffer);
  cgi.view->framebuffer = cg_framebuffer;

  cgi.view->viewport = Vec4i(0, 0, cg_framebuffer->size.w, cg_framebuffer->size.h);

  const player_state_t *ps0;

  if (cgi.client->previous_frame) {
    ps0 = &cgi.client->previous_frame->ps;
  } else {
    cgi.view->flags |= VIEW_FLAG_NO_DELTA;
    ps0 = &frame->ps;
  }

  const player_state_t *ps1 = &frame->ps;

  Cg_PrintControls(ps1);

  Cg_UpdateCameraMode();

  Cg_UpdateOrigin(ps0, ps1);

  Cg_UpdateAngles(ps0, ps1);

  Cg_UpdateThirdPerson(ps1);

  Cg_UpdateFov();

  Cg_UpdateBob(ps1);

  Cg_UpdateAmbient();

  cgi.view->contents = cgi.PointContents(cgi.view->origin);

  cgi.view->ticks = cgi.client->unclamped_time;
}
