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

#pragma once

#include "cg_types.h"

#if defined(__CG_LOCAL_H__)

#define SCORES_COL_WIDTH 240
#define SCORES_ROW_HEIGHT 52
#define SCORES_ICON_WIDTH 48

// the flag a carrier wears in a table row, at the end of the name's column
#define SCORES_BADGE_WIDTH 24

// the name's column in a table row, narrower than a card, so that two teams fit a small display
#define SCORES_NAME_WIDTH 176

// the column the ping keeps at the right of a table row, and the height of its header
#define SCORES_PING_WIDTH 64
#define SCORES_HEADER_HEIGHT 28

// a spectator's row in a table: the icon, the name and the ping, a few to a line
#define SCORES_SPECTATOR_WIDTH (SCORES_ICON_WIDTH + SCORES_NAME_WIDTH + SCORES_PING_WIDTH)
#define SCORES_SPECTATORS_PER_LINE 4
#define SCORES_SPECTATOR_LINES_MAX 2

// the room the spectators take beyond their lines: the gap above them, the heading and the
// spacing between the lines (scoreboard.css)
#define SCORES_SPECTATORS_MARGIN (24 + SCORES_HEADER_HEIGHT + 4)

// the height of a team's total, which heads its column
#define SCORES_TOTAL_HEIGHT 40


void Cg_ParseScores(void);
void Cg_ClearScores(void);

/**
 * @brief The scores as last parsed, sorted, for a module drawing its own board.
 */
const GameScore *Cg_Scores(size_t *count);

/**
 * @brief The scores of the teams in play, as last parsed, in team order, or none outside a team
 * game.
 */
const GameScore *Cg_TeamScores(size_t *count);

/**
 * @brief Incremented whenever the scores change, so a view knows to rebuild.
 */
uint32_t Cg_ScoresGeneration(void);
#endif
