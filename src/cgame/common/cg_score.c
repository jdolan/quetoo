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

typedef struct {
  GameScore scores[MAX_CLIENTS + MAX_TEAMS];
  size_t numScores;

  GameScore pending[MAX_CLIENTS + MAX_TEAMS];
  size_t numPending;

  uint32_t generation;
} CGameScoreState;

static CGameScoreState cgScoreState;

/**
 * @brief A comparator for sorting `GameScore`.
 */
static int32_t Cg_ParseScores_Compare(const void *a, const void *b) {
  const GameScore *sa = (GameScore *) a;
  const GameScore *sb = (GameScore *) b;

  // push spectators to the bottom of the board
  const int16_t s1 = (sa->flags & SCORE_SPECTATOR ? -9999 : sa->score);
  const int16_t s2 = (sb->flags & SCORE_SPECTATOR ? -9999 : sb->score);

  return s2 - s1;
}

/**
 * @brief Parses score data from the server. The scores are sent as binary.
 * If teams play or CTF is enabled, the last two scores in the packet will
 * contain the team scores.
 */
void Cg_ParseScores(void) {

  const int32_t index = cgi.ReadShort();
  const int32_t count = cgi.ReadShort();

  if (index < 0 || count < 0 || index + count > MAX_CLIENTS + MAX_TEAMS) {
    Cg_Error("Invalid score index and count: %d + %d\n", index, count);
  }

  if (index == 0) {
    cgScoreState.numPending = 0;
  } else if ((size_t) index != cgScoreState.numPending) {
    Cg_Warn("Score packet %d arrived with %zu pending\n", index, cgScoreState.numPending);
    cgScoreState.numPending = 0;
    return;
  }

  cgi.ReadData(cgScoreState.pending + index, count * sizeof(GameScore));
  cgScoreState.numPending = index + count;

  if (cgi.ReadByte()) { // last packet in sequence

    cgScoreState.numScores = cgScoreState.numPending;
    cgScoreState.numPending = 0;

    // the aggregate scores are the last set in the array
    if (cgState.numTeams) {
      cgScoreState.numScores -= MAX_TEAMS;
    }

    memcpy(cgScoreState.scores, cgScoreState.pending, sizeof(cgScoreState.scores));

    qsort(cgScoreState.scores, cgScoreState.numScores, sizeof(GameScore), Cg_ParseScores_Compare);

    cgScoreState.generation++;
  }
}

/**
 * @see cg_score.h
 */
const GameScore *Cg_Scores(size_t *count) {
  *count = cgScoreState.numScores;
  return cgScoreState.scores;
}

/**
 * @see cg_score.h
 */
uint32_t Cg_ScoresGeneration(void) {
  return cgScoreState.generation;
}

/**
 * @brief Discards the scores, so that nothing from the last map shows on this one.
 */
void Cg_ClearScores(void) {

  const uint32_t generation = cgScoreState.generation;

  memset(&cgScoreState, 0, sizeof(cgScoreState));

  cgScoreState.generation = generation + 1;
}
