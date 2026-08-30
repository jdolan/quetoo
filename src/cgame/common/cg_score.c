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
  g_score_t scores[MAX_CLIENTS + MAX_TEAMS];
  size_t num_scores;

  g_score_t pending[MAX_CLIENTS + MAX_TEAMS];
  size_t num_pending;
} cg_score_state_t;

static cg_score_state_t cg_score_state;

/**
 * @brief A comparator for sorting `player_score_t`.
 */
static int32_t Cg_ParseScores_Compare(const void *a, const void *b) {
  const g_score_t *sa = (g_score_t *) a;
  const g_score_t *sb = (g_score_t *) b;

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
    cg_score_state.num_pending = 0;
  } else if ((size_t) index != cg_score_state.num_pending) {
    Cg_Warn("Score packet %d arrived with %zu pending\n", index, cg_score_state.num_pending);
    cg_score_state.num_pending = 0;
    return;
  }

  cgi.ReadData(cg_score_state.pending + index, count * sizeof(g_score_t));
  cg_score_state.num_pending = index + count;

  if (cgi.ReadByte()) { // last packet in sequence

    cg_score_state.num_scores = cg_score_state.num_pending;
    cg_score_state.num_pending = 0;

    // the aggregate scores are the last set in the array
    if (cg_state.num_teams) {
      cg_score_state.num_scores -= MAX_TEAMS;
    }

    memcpy(cg_score_state.scores, cg_score_state.pending, sizeof(cg_score_state.scores));

    qsort(cg_score_state.scores, cg_score_state.num_scores, sizeof(g_score_t), Cg_ParseScores_Compare);
  }
}

const g_score_t *Cg_Scores(size_t *count) {
  *count = cg_score_state.num_scores;
  return cg_score_state.scores;
}

/**
 * @brief Discards the scores, so that a board from the previous server is not
 * drawn on the next.
 */
void Cg_ClearScores(void) {

  memset(&cg_score_state, 0, sizeof(cg_score_state));
}

/**
 * @see cg_score.h
 */
int32_t Cg_DrawScoresTitle(void) {
  int32_t ch;

  cgi.BindFont("medium", NULL, &ch);

  const int32_t y = 64 - ch - 4;
  const char *title = cgi.ConfigString(CS_MESSAGE);

  cgi.Draw2DString((cgi.context->w - cgi.StringWidth(title)) / 2, y, title, color_white);

  cgi.BindFont(NULL, NULL, NULL);

  return y + ch;
}

/**
 * @see cg_score.h
 */
int32_t Cg_DrawScoreRow(int32_t x, int32_t y, int32_t width, const g_score_t *s) {
  int32_t cw, ch;

  const cg_client_info_t *info = &cg_state.clients[s->client];

  cgi.Draw2DImage(x + 1, y + 1, SCORES_ICON_WIDTH - 2, SCORES_ICON_WIDTH - 2, info->icon, color_white);

  x += SCORES_ICON_WIDTH;

  const int32_t fw = width - SCORES_ICON_WIDTH - 1;

  if (s->color >= 0) {
    color_t c = ColorHSV(s->color, 1.f, 1.f);
    c.a = s->client == cgi.client->frame.ps.client ? .3f : .15f;

    cgi.Draw2DFill(x, y, fw, SCORES_ROW_HEIGHT - 1, c);
  }

  cgi.BindFont("small", &cw, &ch);

  cgi.Draw2DString(x, y, info->name, color_white);
  cgi.Draw2DString(x + fw + 1 - 6 * cw, y, va("%3dms", s->ping), color_white);

  cgi.BindFont(NULL, NULL, NULL);

  return y + ch;
}

/**
 * @brief Returns the vertical screen coordinate where scores should be drawn.
 */
static int32_t Cg_DrawScoresHeader(void) {
  int32_t cw, ch, x;

  int32_t y = Cg_DrawScoresTitle();

  // team names and scores
  if (cg_state.num_teams) {
    cgi.BindFont("small", &cw, &ch);

    g_score_t *score = &cg_score_state.scores[cg_score_state.num_scores];

    // start from center
    x = cgi.context->w / 2;
    x -= SCORES_COL_WIDTH * (cg_state.num_teams / 2.0);
    x += SCORES_ICON_WIDTH;

    const cg_team_info_t *team = cg_state.teams;
    for (int32_t i = 0; i < cg_state.num_teams; i++, score++, team++) {

#if defined(G_CTF)
      cgi.Draw2DString(x, y, va("%s^7 %d captures", team->name, score->captures), team->color);
#else
      cgi.Draw2DString(x, y, va("%s^7 %d frags", team->name, score->score), team->color);
#endif

      x += SCORES_COL_WIDTH;
    }

    y += ch;
  }

  return y;
}

/**
 * @brief Draws a single player score row including icon, name, frags, deaths, and captures.
 */
static bool Cg_DrawScore(int32_t x, int32_t y, const g_score_t *s) {
  int32_t ch;

#if defined(G_CTF)
  const int32_t top = y;
#endif

  y = Cg_DrawScoreRow(x, y, SCORES_COL_WIDTH, s);

#if defined(G_CTF)
  // flag carrier icon, over the corner of the player's
  if (s->flags & SCORE_CTF_FLAG) {
    const int32_t team = s->team;
    const r_image_t *flag = cgi.LoadImage(va("pics/i_flag%d", team), IMG_PIC);
    cgi.Draw2DImage(x + 1, top + 1, SCORES_ICON_WIDTH * 0.3f, SCORES_ICON_WIDTH * .3f, flag, color_white);
  }
#endif

  x += SCORES_ICON_WIDTH;

  const int32_t fw = SCORES_COL_WIDTH - SCORES_ICON_WIDTH - 1;

  cgi.BindFont("small", NULL, &ch);

  // spectating
  if (s->flags & SCORE_SPECTATOR) {
    cgi.Draw2DString(x, y, "spectating", color_white);
    return true;
  }

  // frags
  cgi.Draw2DString(x, y, va("%d frags", s->score), color_white);

  // deaths
  char *deaths = va("%d deaths ", s->deaths);
  cgi.Draw2DString(x + fw - cgi.StringWidth(deaths), y, deaths, color_white);
  y += ch;

#if defined(G_CTF)
  // captures
  cgi.Draw2DString(x, y, va("%d captures", s->captures), color_white);
#endif

  return true;
}

/**
 * @brief Draws the scores screen layout arranged by team for team-based game modes.
 */
static void Cg_DrawTeamScores(const int32_t start_y) {

  size_t rows = (cgi.context->h - (2 * start_y)) / SCORES_ROW_HEIGHT;
  rows = rows < 3 ? 3 : rows;

  int32_t x = cgi.context->w / 2;
  x -= SCORES_COL_WIDTH * (cg_state.num_teams / 2.0);

  int32_t y = start_y;

  for (int32_t t = 0; t < cg_state.num_teams; t++, x += SCORES_COL_WIDTH, y = start_y) {
    for (size_t i = 0; i < cg_score_state.num_scores; i++) {
      const g_score_t *s = &cg_score_state.scores[i];

      if (s->team != t + 1) {
        continue;
      }

      if (i == rows) {
        break;
      }

      if (Cg_DrawScore(x, y, s)) {
        y += SCORES_ROW_HEIGHT;
      }
    }
  }

  x = cgi.context->w / 2;
  x -= SCORES_COL_WIDTH * (cg_state.num_teams / 2.0);
  x -= SCORES_COL_WIDTH * 2.0;
  y = start_y;

  int32_t j = 0;
  for (size_t i = 0; i < cg_score_state.num_scores; i++) {
    const g_score_t *s = &cg_score_state.scores[i];

    if (!(s->flags & SCORE_SPECTATOR)) {
      continue;
    }

    if (i == rows) {
      break;
    }

    if (Cg_DrawScore(x, y, s)) {
      if (j++ % 2) {
        x -= SCORES_COL_WIDTH;
        y += SCORES_ROW_HEIGHT;
      } else {
        x += SCORES_COL_WIDTH;
      }
    }
  }
}

/**
 * @brief Draws the scores screen layout for deathmatch game modes.
 */
static void Cg_DrawDmScores(const int32_t start_y) {

  size_t rows = (cgi.context->h - (2 * start_y)) / SCORES_ROW_HEIGHT;
  rows = rows < 3 ? 3 : rows;

  const size_t cols = (rows < cg_score_state.num_scores) ? 2 : 1;
  const size_t width = cols * SCORES_COL_WIDTH;

  const g_score_t *s = cg_score_state.scores;
  for (size_t i = 0; i < cg_score_state.num_scores; i++, s++) {

    if (i == (cols * rows)) { // screen is full
      break;
    }

    const size_t col = i / rows;

    const int32_t x = (int32_t) (cgi.context->w / 2 - width / 2 + col * SCORES_COL_WIDTH);
    const int32_t y = (int32_t) (start_y + (i % rows) * SCORES_ROW_HEIGHT);

    if (!Cg_DrawScore(x, y, s)) {
      i--;
    }
  }
}

/**
 * @brief The tail of the `Cg_DrawScores` hook: the frags, deaths and teams.
 */
static void Cg_DrawScores_Common(const player_state_t *ps) {

  if (!ps->stats[STAT_SCORES]) {
    return;
  }

  if (!cg_score_state.num_scores) {
    return;
  }

  const int32_t start_y = Cg_DrawScoresHeader();

  if (cg_state.num_teams) {
    Cg_DrawTeamScores(start_y);
  } else {
    Cg_DrawDmScores(start_y);
  }
}

DrawScores Cg_DrawScores = Cg_DrawScores_Common;
