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

#define RACE_SCORES_COL_WIDTH 280
#define RACE_SCORES_ROW_HEIGHT 48
#define RACE_SCORES_ICON_WIDTH 48

// the most records the config string carries, as the server publishes it
#define RACE_SCORES_RECORDS 15

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
 * @brief The map's title across the top, as the common board has it.
 * @return The y beneath it.
 */
static int32_t Cg_Race_DrawScoresHeader(void) {
  int32_t ch;

  cgi.BindFont("medium", NULL, &ch);

  const int32_t y = 64 - ch - 4;
  const char *title = cgi.ConfigString(CS_MESSAGE);

  cgi.Draw2DString((cgi.context->w - cgi.StringWidth(title)) / 2, y, title, color_white);

  cgi.BindFont(NULL, NULL, NULL);

  return y + ch;
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
  for (int32_t rank = 1; rank <= RACE_SCORES_RECORDS && *s; rank++) {

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
 * @brief One racer: their icon and name with their ping, then what they are
 * doing, their best and their runs.
 */
static void Cg_Race_DrawScore(int32_t x, int32_t y, const g_score_t *s) {
  int32_t cw, ch;

  const cg_client_info_t *info = &cg_state.clients[s->client];

  cgi.Draw2DImage(x + 1, y + 1, RACE_SCORES_ICON_WIDTH - 2, RACE_SCORES_ICON_WIDTH - 2, info->icon, color_white);

  x += RACE_SCORES_ICON_WIDTH;

  const int32_t fw = RACE_SCORES_COL_WIDTH - RACE_SCORES_ICON_WIDTH - 1;

  if (s->color >= 0) {
    color_t c = ColorHSV(s->color, 1.f, 1.f);
    c.a = s->client == cgi.client->frame.ps.client ? .3f : .15f;

    cgi.Draw2DFill(x, y, fw, RACE_SCORES_ROW_HEIGHT - 1, c);
  }

  cgi.BindFont("small", &cw, &ch);

  cgi.Draw2DString(x, y, info->name, color_white);
  cgi.Draw2DString(x + fw - 5 * cw, y, va("%3dms", s->ping), color_white);
  y += ch;

  cgi.Draw2DString(x, y, Cg_Race_ModeName(s->race_mode), color_white);

  if (s->race_mode != RACE_MODE_SPECTATOR) {
    const char *best = s->race_best ? Cg_Race_FormatTime(s->race_best) : "no time";
    const char *right = va("%s  %u run%s", best, s->race_runs, s->race_runs == 1 ? "" : "s");

    cgi.Draw2DString(x + fw - cgi.StringWidth(right), y, right, color_white);
  }

  cgi.BindFont(NULL, NULL, NULL);
}

void Cg_Race_DrawScores(const player_state_t *ps) {

  if (!ps->stats[STAT_SCORES]) {
    return;
  }

  const int32_t start_y = Cg_Race_DrawScoresHeader();
  const int32_t gap = RACE_SCORES_ICON_WIDTH / 2;

  const int32_t left = cgi.context->w / 2 - RACE_SCORES_COL_WIDTH - gap / 2;
  const int32_t right = cgi.context->w / 2 + gap / 2;

  Cg_Race_DrawRecords(left, start_y);

  size_t count;
  const g_score_t *scores = Cg_Scores(&count);

  // a second column of racers opens to the right once the first is full
  const size_t rows = Maxz(3, (cgi.context->h - 2 * start_y) / RACE_SCORES_ROW_HEIGHT);

  for (size_t i = 0; i < count && i < 2 * rows; i++) {
    const int32_t x = right + (int32_t) (i / rows) * (RACE_SCORES_COL_WIDTH + gap);
    const int32_t y = start_y + (int32_t) (i % rows) * RACE_SCORES_ROW_HEIGHT;

    Cg_Race_DrawScore(x, y, &scores[i]);
  }
}
