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

#include "CounterView.h"

/**
 * @file
 * @brief One team's score beside the clock, in team play.
 */

typedef struct TeamScoreView TeamScoreView;
typedef struct TeamScoreViewInterface TeamScoreViewInterface;

/**
 * @brief One team's score beside the clock, in team play: the team's name over its score, in the
 * team's color, from the team's entry in `CS_TEAM_STATE`.
 * @details Configured in JSON by `team`, from 1. Teams 1 and 3 face teams 2 and 4 across the
 * clock: a team that the level lacks keeps its place while the team that faces it plays, so that
 * the clock stays centered. Hidden if neither plays.
 * @extends CounterView
 */
struct TeamScoreView {

  /**
   * @brief The superclass.
   */
  CounterView counterView;

  /**
   * @brief The interface type.
   * @protected
   */
  TeamScoreViewInterface *interface[0];

  /**
   * @brief The team, from 1.
   */
  int32_t team;

  /**
   * @brief True if the level has this team.
   */
  bool plays;

  /**
   * @brief The team's entry in `CS_TEAM_STATE`: its score, then after a ':' what a feature adds.
   * Empty if the team does not play.
   */
  char state[64];

  /**
   * @brief The score's color, transparent until it is set.
   * @private
   */
  SDL_Color color;
};

struct TeamScoreViewInterface {

  /**
   * @brief The superclass interface.
   */
  CounterViewInterface counterViewInterface;

  /**
   * @fn const char *TeamScoreView::captionForTeam(const TeamScoreView *self, const CGameTeamInfo *team)
   * @brief Resolves the caption over the score.
   * @details The default is the team's name. A feature that appends its own state to the team's
   * entry MAY show that instead.
   * @param self The TeamScoreView.
   * @param team The team.
   * @return The caption.
   * @memberof TeamScoreView
   */
  const char *(*captionForTeam)(const TeamScoreView *self, const CGameTeamInfo *team);
};

CGAME_EXPORT Class *_TeamScoreView(void);
