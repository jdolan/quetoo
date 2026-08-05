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

#pragma once

#include "cg_types.h"

typedef enum {
  CROSSHAIR_HEALTH_NONE,
  CROSSHAIR_HEALTH_RED_WHITE,
  CROSSHAIR_HEALTH_RED_WHITE_GREEN,
  CROSSHAIR_HEALTH_RED_YELLOW_WHITE,
  CROSSHAIR_HEALTH_RED_YELLOW_WHITE_GREEN,
  CROSSHAIR_HEALTH_WHITE_GREEN
} cg_crosshair_health_t;

#define CROSSHAIR_HEALTH_FIRST CROSSHAIR_HEALTH_NONE
#define CROSSHAIR_HEALTH_LAST CROSSHAIR_HEALTH_WHITE_GREEN

#define CROSSHAIR_SCALE 0.125f
#define CROSSHAIR_PULSE_ALPHA 0.5f

/**
 * @brief The running position of each column of the HUD that stacks, carried
 * through the arrangement so that no element has to know how many drew above it.
 * @details The two stacking columns are the powerups down the left of the view and
 * the stat rows down the right. Everything else in the HUD places itself, and
 * takes no position.
 *
 * This exists because the stat rows used to address their slot arithmetically -
 * frags at one row, deaths at two, captures at three - which meant an element that
 * a feature drew had to be paid for by an element that did not know the feature
 * existed: the match time subtracted a row of its own under `G_CTF`. A cursor each
 * feature advances says the same thing without anybody naming anybody else.
 */
typedef struct {
  /**
   * @brief The y of the next powerup slot.
   */
  int32_t powerup_y;

  /**
   * @brief The y of the next stat row.
   */
  int32_t stat_y;
} cg_hud_layout_t;

#if defined(__CG_LOCAL_H__)

void Cg_DrawHud(const player_state_t *ps);

/**
 * @defgroup hud-elements HUD elements
 * @brief The building blocks of the HUD, public so that a module may arrange them
 * as it likes rather than only insert into the arrangement common ships.
 *
 * An element that stacks takes the y of its slot and returns the y of the next
 * one, which is `Cg_DrawPowerup`'s shape. The rest place themselves from the
 * context and take no position, because a coordinate they would ignore is a
 * signature that lies about what it does.
 * @{
 */

/**
 * @brief Draws the powerups the client holds, and the time left on each.
 * @return The y of the next powerup slot.
 */
int32_t Cg_DrawPowerups(const player_state_t *ps, int32_t y);

/**
 * @brief Draws the client's frag count.
 * @return The y of the next stat row.
 * @details The row is reserved whether or not it draws, so that a spectator sees
 * the rows below it where a player would.
 */
int32_t Cg_DrawFrags(const player_state_t *ps, int32_t y);

/**
 * @brief Draws the client's death count.
 * @return The y of the next stat row, reserved as `Cg_DrawFrags` reserves it.
 */
int32_t Cg_DrawDeaths(const player_state_t *ps, int32_t y);

/**
 * @brief Draws the time left in the match, or the time elapsed.
 * @return The y below the line drawn, which is one line rather than a whole row.
 */
int32_t Cg_DrawTime(const player_state_t *ps, int32_t y);

void Cg_DrawVitals(const player_state_t *ps);
void Cg_DrawPickup(const player_state_t *ps);
void Cg_DrawSpectator(const player_state_t *ps);
void Cg_DrawChase(const player_state_t *ps);
void Cg_DrawTeamBanner(const player_state_t *ps);
void Cg_DrawDamageInflicted(const player_state_t *ps);

/**
 * @}
 */
#endif /* __CG_LOCAL_H__ */
