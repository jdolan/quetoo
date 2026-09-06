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
 * @brief Draws the "Spectating" label when the player is a spectator not in chase mode.
 */
void Cg_DrawSpectator(const player_state_t *ps) {
  int32_t x, y, cw;

  if (!ps->stats[STAT_SPECTATOR] || ps->stats[STAT_CHASE]) {
    return;
  }

  cgi.BindFont("small", &cw, NULL);

  x = cgi.context->w - cgi.StringWidth("Spectating");
  y = HUD_PIC_HEIGHT;

  cgi.Draw2DString(x, y, "Spectating", color_green);

  cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the name of the player currently being chased in chasecam mode.
 */
void Cg_DrawChase(const player_state_t *ps) {
  int32_t x, y, ch;
  char string[MAX_INFO_STRING_VALUE * 2], *s;

  // if we've changed chase targets, reset the HUD
  if (ps->stats[STAT_CHASE] != cg_hud_state.chase_target) {
    memset(&cg_hud_state, 0, sizeof(cg_hud_state));
    cg_hud_state.chase_target = ps->stats[STAT_CHASE];
  }

  if (!ps->stats[STAT_CHASE]) {
    return;
  }

  const int32_t e = ps->stats[STAT_CHASE];

  if (e < 0 || e >= MAX_ENTITIES) {
    Cg_Warn("Invalid client info index: %d\n", e);
    return;
  }

  cl_entity_t *ent = cgi.client->entities + e;

  const cg_client_info_t *ci = &cg_state.clients[ent->current.client];

  cgi.BindFont("small", NULL, &ch);

  q_snprintf(string, sizeof(string), "Chasing ^7%s", ci->name);

  if ((s = q_strchr(string, '\\'))) {
    *s = '\0';
  }

  x = cgi.context->w * 0.5 - cgi.StringWidth(string) / 2;
  y = cgi.context->h - HUD_PIC_HEIGHT - ch;

  cgi.Draw2DString(x, y, string, color_green);

  cgi.BindFont(NULL, NULL, NULL);
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
 * @brief The overlays still drawn through r_draw_2d, beneath what a module adds.
 */
static void Cg_DrawHudElements_Common(const player_state_t *ps) {

  Cg_DrawSpectator(ps);

  Cg_DrawChase(ps);
}

DrawHudElements Cg_DrawHudElements = Cg_DrawHudElements_Common;

/**
 * @brief Hands the frame to the HUD View hierarchy, which resolves its own visibility.
 */
void Cg_UpdateHud(const cl_frame_t *frame) {

  if (cg_hud_view_controller) {
    $(cg_hud_view_controller, updateWithFrame, frame);
  }
}

/**
 * @brief The default HUD needs nothing beyond its JSON; modules chain onto this.
 */
static void Cg_ConfigureHud_Common(View *hud) {

}

ConfigureHud Cg_ConfigureHud = Cg_ConfigureHud_Common;

/**
 * @brief Draws what the HUD still draws through r_draw_2d; the View hierarchy is updated by
 * Cg_UpdateHud, whether or not this runs.
 */
void Cg_DrawHud(const cl_frame_t *frame) {

  const player_state_t *ps = &frame->ps;

  if (!cg_draw_hud->integer) {
    return;
  }

  if (!ps->stats[STAT_TIME]) { // intermission
    return;
  }

  if (editor->value) {
    return;
  }

  Cg_DrawHudElements(ps);

  Cg_Vote_Draw();

  Cg_DrawCenterPrint(ps);

  Cg_DrawTargetName(ps);

  Cg_DrawBlend(ps);

  Cg_DrawDamageInflicted(ps);
}
