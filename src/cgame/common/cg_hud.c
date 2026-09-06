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
#include "cg_hud_draw.h"

#include "ui/hud/HudViewController.h"

/**
 * @brief Resets the HUD state when the chase target changes, so nothing carries over.
 */
static void Cg_UpdateChase(const player_state_t *ps) {

  if (ps->stats[STAT_CHASE] != cg_hud_state.chase_target) {
    memset(&cg_hud_state, 0, sizeof(cg_hud_state));
    cg_hud_state.chase_target = ps->stats[STAT_CHASE];
  }
}

/**
 * @brief Plays the hit sound if the player inflicted damage this frame.
 */
void Cg_DrawDamageInflicted(const player_state_t *ps) {

  if (!cg_hit_sound->integer) {
    return;
  }

  const int16_t dmg = ps->stats[STAT_DAMAGE_INFLICT];
  if (dmg) {

    // play the hit sound
    if (cgi.client->unclamped_time - cg_hud_state.damage.hit_sound_time > 50) {
      cg_hud_state.damage.hit_sound_time = cgi.client->unclamped_time;

      Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
        .sample = dmg >= 25 ? cg_sample_hits[1] : cg_sample_hits[0],
        .entity = Cg_Self()
      });
    }
  }
}

/**
 * @brief Hands the frame to the HUD View hierarchy, which resolves its own visibility.
 */
void Cg_UpdateHud(const cl_frame_t *frame) {

  if (cg_hud_view_controller) {
    $(cg_hud_view_controller, updateWithFrame, frame);
  }
}

/**
 * @brief What the HUD still does outside its View hierarchy each frame: the hit sound.
 */
void Cg_DrawHud(const cl_frame_t *frame) {

  const player_state_t *ps = &frame->ps;

  Cg_UpdateChase(ps);

  if (!cg_draw_hud->integer) {
    return;
  }

  if (!ps->stats[STAT_TIME]) { // intermission
    return;
  }

  if (editor->value) {
    return;
  }

  Cg_DrawDamageInflicted(ps);
}
