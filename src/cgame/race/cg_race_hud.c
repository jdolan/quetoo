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

#include "cg_race.h"

/**
 * @file
 * @brief The run on the HUD: the time across the top with the checkpoints
 * beneath it, and the speed under the crosshair. Everything here is read from
 * the player state the server already sends, or measured on the client; the
 * speed in particular is the client's own, not the server's word for it.
 */

// where the run sits, from the top of the view
#define RACE_HUD_TOP 32

// where the speed sits, from the middle of the view
#define RACE_HUD_SPEED_OFFSET 48

// how much of a frame's speed the readout takes on: it flickers at raw
#define RACE_HUD_SPEED_LERP .2f

static float cg_race_speed;

static const char *Cg_Race_FormatTime(uint32_t ms) {
  return va("%u:%02u.%03u", ms / 60000, ms / 1000 % 60, ms % 1000);
}

static void Cg_Race_DrawCentered(int32_t y, const char *string, const color_t color) {
  cgi.Draw2DString((cgi.context->w - cgi.StringWidth(string)) / 2, y, string, color);
}

/**
 * @brief The time, colored by what will become of it, and the checkpoints
 * reached out of the course's.
 */
static void Cg_Race_DrawRun(const player_state_t *ps) {

  const g_race_run_state_t state = ps->stats[STAT_RACE_RUN];
  if (state == RACE_RUN_IDLE) {
    return;
  }

  color_t color = color_white;
  if (ps->stats[STAT_RACE_FLAGS]) {
    color = color_red;
  } else if (ps->stats[STAT_RACE_MODE] == RACE_MODE_PRACTICE) {
    color = color_yellow;
  } else if (state == RACE_RUN_FINISHED) {
    color = color_green;
  }

  int32_t ch, y = RACE_HUD_TOP;

  cgi.BindFont("large", NULL, &ch);
  Cg_Race_DrawCentered(y, Cg_Race_FormatTime(Cg_Race_Time(ps)), color);
  y += ch;

  uint32_t checkpoints = 0;
  sscanf(cgi.ConfigString(CS_RACE_COURSE), "%u", &checkpoints);

  if (checkpoints) {
    cgi.BindFont("small", NULL, &ch);
    Cg_Race_DrawCentered(y, va("%d / %u", ps->stats[STAT_RACE_CHECKPOINTS], checkpoints), color_white);
  }

  cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief The horizontal speed, smoothed over frames, beneath the crosshair.
 */
static void Cg_Race_DrawSpeed(const player_state_t *ps) {

  vec3_t velocity = ps->pm_state.velocity;
  velocity.z = 0.f;

  cg_race_speed += (Vec3_Length(velocity) - cg_race_speed) * RACE_HUD_SPEED_LERP;

  cgi.BindFont("medium", NULL, NULL);
  Cg_Race_DrawCentered(cgi.context->h / 2 + RACE_HUD_SPEED_OFFSET, va("%.0f", cg_race_speed), color_white);
  cgi.BindFont(NULL, NULL, NULL);
}

void Cg_Race_DrawHud(const player_state_t *ps) {

  if (ps->stats[STAT_RACE_MODE] == RACE_MODE_SPECTATOR) {
    return;
  }

  Cg_Race_DrawRun(ps);
  Cg_Race_DrawSpeed(ps);
}
