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
 * @brief The HUD, arranged for racing. Health, ammo, armor and the powerups stay
 * as common draws them - a rocket jump wants both - and the frags and deaths
 * go, since nobody is scoring any; in their column are the speed and the runs
 * so far. The run's time sits across the top with the checkpoints beneath it.
 *
 * Everything here is read from the player state the server already sends, or
 * measured on the client; the speed in particular is the client's own, not
 * the server's word for it.
 */

// where the run sits, from the top of the view
#define RACE_HUD_TOP 32

// how much of each frame's speed the readout takes on; shown raw, it flickers
// with every frame at a high frame rate
#define RACE_HUD_SPEED_LERP .2f

static float cg_race_speed;

/**
 * @see cg_race.h
 */
const char *Cg_Race_FormatTime(uint32_t ms) {
  return va("%u:%02u.%03u", ms / 60000, ms / 1000 % 60, ms % 1000);
}

/**
 * @brief Draws a line centered on the view, as the run and its milestones are shown.
 */
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
 * @brief The horizontal speed, smoothed over frames.
 */
static int32_t Cg_Race_DrawSpeed(const player_state_t *ps, int32_t y) {

  vec3_t velocity = ps->pm_state.velocity;
  velocity.z = 0.f;

  cg_race_speed += (Vec3_Length(velocity) - cg_race_speed) * RACE_HUD_SPEED_LERP;

  return Cg_DrawStat(y, "Speed", (int32_t) cg_race_speed);
}

/**
 * @see cg_race.h
 */
void Cg_Race_DrawHud(const player_state_t *ps, cg_hud_layout_t *layout) {

  Cg_DrawVitals(ps);

  layout->powerup_y = Cg_DrawPowerups(ps, layout->powerup_y);

  Cg_DrawPickup(ps);

  Cg_DrawSpectator(ps);

  Cg_DrawChase(ps);

  if (ps->stats[STAT_RACE_MODE] == RACE_MODE_SPECTATOR) {
    return;
  }

  Cg_Race_DrawRun(ps);

  layout->stat_y = Cg_Race_DrawSpeed(ps, layout->stat_y);
  layout->stat_y = Cg_DrawStat(layout->stat_y, "Runs", ps->stats[STAT_RACE_RUNS]);
}
