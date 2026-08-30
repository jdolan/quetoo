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

#if defined(__CG_LOCAL_H__)

#define SCORES_COL_WIDTH 240
#define SCORES_ROW_HEIGHT 48
#define SCORES_ICON_WIDTH 48

void Cg_ParseScores(void);
void Cg_ClearScores(void);

/**
 * @brief The scores as last parsed, sorted, for a module drawing its own board.
 */
const g_score_t *Cg_Scores(size_t *count);

/**
 * @brief Draws the map's title across the top of the board.
 * @return The y beneath it.
 */
int32_t Cg_DrawScoresTitle(void);

/**
 * @brief Draws what every board's row begins with: the icon, the team fill
 * across `width`, the name and the ping.
 * @return The y of the row's second line, whose x is `x + SCORES_ICON_WIDTH`.
 */
int32_t Cg_DrawScoreRow(int32_t x, int32_t y, int32_t width, const g_score_t *s);
#endif
