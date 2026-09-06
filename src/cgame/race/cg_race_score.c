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

#include "ui/hud/OverlayText.h"
#include "ui/hud/ScoreboardView.h"

/**
 * @file
 * @brief The scoreboard, arranged for racing: the course records down the left,
 * from `CS_RACE_RECORDS`, and the racers down the right, each with their mode,
 * their best on this map and their runs, in the order the server ranked them.
 * Race's `ui/hud/scoreboard.json` names RaceScoreboardView in place of the stock board.
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

#pragma mark - RecordsView

#define _Class _RecordsView

/**
 * @brief The course records: rank, name and time, one a line, from `CS_RACE_RECORDS`.
 * @extends OverlayText
 */
typedef struct RecordsViewInterface RecordsViewInterface;

typedef struct {
  OverlayText overlayText;
  RecordsViewInterface *interface[0];
} RecordsView;

struct RecordsViewInterface {
  OverlayTextInterface overlayTextInterface;
};

/**
 * @see OverlayText::textForFrame(OverlayText *, const cl_frame_t *)
 */
static const char *textForFrame(OverlayText *self, const cl_frame_t *frame) {

  static char text[MAX_STRING_CHARS * 2];
  char string[MAX_STRING_CHARS];

  q_strlcpy(string, cgi.ConfigString(CS_RACE_RECORDS), sizeof(string));
  q_strlcpy(text, "^2Course records", sizeof(text));

  if (!*string) {
    q_strlcat(text, "\n^8none yet", sizeof(text));
    return text;
  }

  // the rows are monospaced, so the times right-align by padding the names
  const int32_t width = RACE_SCORES_COL_WIDTH / 14;

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
    const int32_t pad = width - 4 - (int32_t) q_strlen(name) - (int32_t) q_strlen(formatted);

    q_strlcat(text, va("\n^7%2d  %s%*s%s", rank, name, pad > 1 ? pad : 1, "", formatted), sizeof(text));
  }

  return text;
}

/**
 * @see Class::initialize(Class *)
 */
static void initializeRecordsView(Class *clazz) {
  ((OverlayTextInterface *) clazz->interface)->textForFrame = textForFrame;
}

Class *_RecordsView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "RecordsView",
      .superclass = _OverlayText(),
      .instanceSize = sizeof(RecordsView),
      .interfaceSize = sizeof(RecordsViewInterface),
      .initialize = initializeRecordsView,
    });
  });

  return clazz;
}

#undef _Class

#pragma mark - RaceScoreboardView

#define _Class _RaceScoreboardView

/**
 * @brief The scoreboard for racing: wider rows showing the mode and the best time, with the
 * course records in a column of their own, leading.
 * @extends ScoreboardView
 */
typedef struct RaceScoreboardViewInterface RaceScoreboardViewInterface;

typedef struct {
  ScoreboardView scoreboardView;
  RaceScoreboardViewInterface *interface[0];
} RaceScoreboardView;

struct RaceScoreboardViewInterface {
  ScoreboardViewInterface scoreboardViewInterface;
};

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = super(View, self, init);
  if (self) {
    ((ScoreboardView *) self)->rowWidth = RACE_SCORES_COL_WIDTH;
  }

  return self;
}

/**
 * @see ScoreboardView::rebuild(ScoreboardView *)
 */
static void rebuild(ScoreboardView *self) {

  super(ScoreboardView, self, rebuild);

  const Array *columns = (Array *) self->columns->view.subviews;
  const size_t count = columns->count;

  StackView *records = $(self, addColumn);
  $((View *) records, addClassName, "records");

  Text *text = $((Text *) alloc(RecordsView), initWithText, NULL, NULL);
  assert(text);

  $((View *) records, addSubview, (View *) text);
  release(text);

  // the records lead; the racers follow in the order the board built them
  for (size_t i = 0; i < count; i++) {
    View *column = $(columns, objectAtIndex, 0);
    $((View *) self->columns, bringSubviewToFront, column);
  }
}

/**
 * @see ScoreboardView::rowForScore(ScoreboardView *, const g_score_t *)
 */
static ScoreRowView *rowForScore(ScoreboardView *self, const g_score_t *score) {

  ScoreRowView *row = $(alloc(ScoreRowView), initWithScore, score, self->rowWidth);
  assert(row);

  const char *aside = NULL;

  if (score->race_mode != RACE_MODE_SPECTATOR) {
    const char *best = score->race_best ? Cg_Race_FormatTime(score->race_best) : "no time";
    aside = va("%s  %u run%s", best, score->race_runs, score->race_runs == 1 ? "" : "s");
  }

  $(row, setDetails, Cg_Race_ModeName(score->race_mode), aside);

  return row;
}

/**
 * @see Class::initialize(Class *)
 */
static void initializeRaceScoreboardView(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;

  ((ScoreboardViewInterface *) clazz->interface)->rebuild = rebuild;
  ((ScoreboardViewInterface *) clazz->interface)->rowForScore = rowForScore;
}

Class *_RaceScoreboardView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "RaceScoreboardView",
      .superclass = _ScoreboardView(),
      .instanceSize = sizeof(RaceScoreboardView),
      .interfaceSize = sizeof(RaceScoreboardViewInterface),
      .initialize = initializeRaceScoreboardView,
    });
  });

  return clazz;
}

#undef _Class
