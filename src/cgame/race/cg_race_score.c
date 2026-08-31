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
 * @brief The scoreboard, arranged for racing: the course records down the left,
 * from `CS_RACE_RECORDS`, and the racers down the right, each with their mode,
 * their best on this map and their runs, in the order the server ranked them.
 */

// wider than the common board's column, for a best time beside a run count
#define RACE_SCORES_COL_WIDTH 280

/**
 * @brief What a racer is doing, as the board says it.
 */
static const char *Cg_Race_ModeName(g_race_mode_t mode) {

  switch (mode) {
    case RACE_MODE_RACE:
      return "racing";
    case RACE_MODE_PRACTICE:
      return "practicing";
    default:
      return "spectating";
  }
}

/**
 * @brief The course records, one a row: rank, name and time.
 */
static void Cg_Race_DrawRecords(int32_t x, int32_t y) {
  char string[MAX_STRING_CHARS];
  int32_t ch;

  q_strlcpy(string, cgi.ConfigString(CS_RACE_RECORDS), sizeof(string));

  cgi.BindFont("small", NULL, &ch);

  cgi.Draw2DString(x, y, "Course records", color_green);
  y += ch;

  if (!*string) {
    cgi.Draw2DString(x, y, "none yet", color_grey);
    cgi.BindFont(NULL, NULL, NULL);
    return;
  }

  char *s = string;
  for (int32_t rank = 1; rank <= RACE_RECORDS_SHOWN && *s; rank++) {

    char *name = s;
    char *time = strchr(name, '\\');
    if (!time) {
      break;
    }
    *time++ = '\0';

    s = strchr(time, '\\');
    if (s) {
      *s++ = '\0';
    } else {
      s = time + q_strlen(time);
    }

    const char *formatted = Cg_Race_FormatTime((uint32_t) strtoul(time, NULL, 10));

    cgi.Draw2DString(x, y, va("%2d  %s", rank, name), color_white);
    cgi.Draw2DString(x + RACE_SCORES_COL_WIDTH - cgi.StringWidth(formatted), y, formatted, color_white);
    y += ch;
  }

  cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief One racer: the row every board begins with, then what they are doing,
 * their best and their runs.
 */
static void Cg_Race_DrawScore(int32_t x, int32_t y, const g_score_t *s) {

  y = Cg_DrawScoreRow(x, y, RACE_SCORES_COL_WIDTH, s);
  x += SCORES_ICON_WIDTH;

  const int32_t fw = RACE_SCORES_COL_WIDTH - SCORES_ICON_WIDTH - 1;

  cgi.BindFont("small", NULL, NULL);

  cgi.Draw2DString(x, y, Cg_Race_ModeName(s->race_mode), color_white);

  if (s->race_mode != RACE_MODE_SPECTATOR) {
    const char *best = s->race_best ? Cg_Race_FormatTime(s->race_best) : "no time";
    const char *right = va("%s  %u run%s", best, s->race_runs, s->race_runs == 1 ? "" : "s");

    cgi.Draw2DString(x + fw - cgi.StringWidth(right), y, right, color_white);
  }

  cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @see cg_race.h
 */
void Cg_Race_DrawScores(const player_state_t *ps) {

  if (!ps->stats[STAT_SCORES]) {
    return;
  }

  const int32_t start_y = Cg_DrawScoresTitle();
  const int32_t gap = SCORES_ICON_WIDTH / 2;

  const int32_t left = cgi.context->w / 2 - RACE_SCORES_COL_WIDTH - gap / 2;
  const int32_t right = cgi.context->w / 2 + gap / 2;

  Cg_Race_DrawRecords(left, start_y);

  size_t count;
  const g_score_t *scores = Cg_Scores(&count);

  // a second column of racers opens to the right once the first is full
  const size_t rows = Maxz(3, (cgi.context->h - 2 * start_y) / SCORES_ROW_HEIGHT);

  for (size_t i = 0; i < count && i < 2 * rows; i++) {
    const int32_t x = right + (int32_t) (i / rows) * (RACE_SCORES_COL_WIDTH + gap);
    const int32_t y = start_y + (int32_t) (i % rows) * SCORES_ROW_HEIGHT;

    Cg_Race_DrawScore(x, y, &scores[i]);
  }
}
