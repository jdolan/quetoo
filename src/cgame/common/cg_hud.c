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
#include "cg_hud_draw.h"

#include "ui/hud/HudViewController.h"

/**
 * @brief Resets the HUD state when the chase target changes, so nothing carries over.
 */
static void Cg_UpdateChase(const PlayerState *ps) {

  if (ps->stats[STAT_CHASE] != cgHudState.chaseTarget) {
    Cg_ClearHud();
    cgHudState.chaseTarget = ps->stats[STAT_CHASE];
  }
}

/**
 * @brief Plays the hit sound if the player inflicted damage this frame.
 */
static void Cg_DrawDamageInflicted(const PlayerState *ps) {

  if (!cg_hitSound->integer) {
    return;
  }

  const int16_t dmg = ps->stats[STAT_DAMAGE_INFLICT];
  if (dmg) {
    if (cgi.client->unclampedTime - cgHudState.damage.hitSoundTime > 50) {
      cgHudState.damage.hitSoundTime = cgi.client->unclampedTime;

      Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
        .sample = dmg >= 25 ? cgSampleHits[1] : cgSampleHits[0],
        .entity = Cg_Self()
      });
    }
  }
}

/**
 * @brief Hands the frame to the HUD View hierarchy, which resolves its own visibility.
 */
void Cg_UpdateHud(const ClientFrame *frame) {

  if (cgHudViewController) {
    $(cgHudViewController, updateWithFrame, frame);
  }
}

/**
 * @brief What the HUD still does outside its View hierarchy each frame: the hit sound.
 */
void Cg_DrawHud(const ClientFrame *frame) {

  const PlayerState *ps = &frame->ps;

  Cg_UpdateChase(ps);

  if (!cg_drawHud->integer) {
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
