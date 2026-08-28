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

#include "cg_local.h"

/**
 * @file
 * @brief Racing, as the client shows it: the run on the HUD, and the course
 * record's ghost dressed as its holder. `cg_race.c` installs the hooks;
 * `cg_race_hud.c` draws.
 */

/**
 * @brief Installs racing over the hooks it needs, once per module image.
 */
void Cg_Race_Init(void);

/**
 * @brief The run's time from the two stats it is split across.
 */
uint32_t Cg_Race_Time(const player_state_t *ps);

/**
 * @brief Formats a run time as the HUD and the board show it.
 */
const char *Cg_Race_FormatTime(uint32_t ms);

/**
 * @brief The scoreboard, arranged for racing. Installed over `DrawScores` by
 * `Cg_Race_Init`, not chained.
 */
void Cg_Race_DrawScores(const player_state_t *ps);

/**
 * @brief The whole HUD, arranged for racing: what common draws that a racer
 * needs, and the run in place of the frags and deaths. Installed over
 * `DrawHudElements` by `Cg_Race_Init`, not chained.
 */
void Cg_Race_DrawHud(const player_state_t *ps, cg_hud_layout_t *layout);
