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

#include "HudViewController.h"
#include "ScoreboardView.h"

#define _Class _ScoreboardView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  ScoreboardView *this = (ScoreboardView *) self;

  release(this->columns);
  release(this->header);
  release(this->title);

  super(Object, self, dealloc);
}

#pragma mark - Rows

/**
 * @brief Appends a row for `score` to `column`.
 */
static void addRow(ScoreboardView *self, StackView *column, const g_score_t *score) {

  ScoreView *row = $(self, scoreView, score);
  assert(row);

  $((View *) column, addSubview, (View *) row);
  release(row);
}

/**
 * @brief The stock detail lines: frags and deaths, or spectating; in CTF, captures too.
 */
static void configureScoreRow(ScoreView *row, const g_score_t *score) {

  if (score->flags & SCORE_SPECTATOR) {
    $(row, setDetails, "spectating", NULL);
    return;
  }

#if defined(G_CTF)
  $(row, setDetails, va("%d frags\n%d captures", score->score, score->captures), va("%d deaths", score->deaths));

  if (score->flags & SCORE_CTF_FLAG) {
    $(row->badge, setImage, (Image *) Cg_HudImage(va("pics/flag%d", score->team)));
    $((View *) row->badge, setHidden, false);
  }
#else
  $(row, setDetails, va("%d frags", score->score), va("%d deaths", score->deaths));
#endif
}

/**
 * @brief The stock team total.
 */
static const char *teamTotal(const cg_team_info_t *team, const g_score_t *score) {
#if defined(G_CTF)
  return va("%s^7 %d captures", team->name, score->captures);
#else
  return va("%s^7 %d frags", team->name, score->score);
#endif
}

/**
 * @brief The rows that fit beneath `top`, at least three.
 */
static size_t rowsThatFit(const ScoreboardView *self, int32_t top) {
  return (size_t) Maxi(3, (self->view.frame.h - 2 * top) / SCORES_ROW_HEIGHT);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = $(self, initWithFrame, NULL);
  if (self) {
    ScoreboardView *this = (ScoreboardView *) self;

    this->rowWidth = SCORES_COL_WIDTH;

    this->title = $(alloc(Text), initWithText, NULL, NULL);
    assert(this->title);

    $((View *) this->title, addClassName, "title");
    $(self, addSubview, (View *) this->title);

    this->header = $(alloc(StackView), initWithFrame, NULL);
    assert(this->header);

    this->header->axis = StackViewAxisHorizontal;
    $((View *) this->header, addClassName, "header");
    $(self, addSubview, (View *) this->header);

    this->columns = $(alloc(StackView), initWithFrame, NULL);
    assert(this->columns);

    this->columns->axis = StackViewAxisHorizontal;
    $((View *) this->columns, addClassName, "columns");
    $(self, addSubview, (View *) this->columns);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  ScoreboardView *this = (ScoreboardView *) self;

  if (data) {
    const uint32_t generation = Cg_ScoresGeneration();

    // Rebuilt before the recursion, so that new rows take this frame too
    if (generation != this->generation || self->frame.h != this->height) {
      this->generation = generation;
      this->height = self->frame.h;

      $(this, rebuild);
    }
  }

  super(View, self, updateBindings, data);
}

#pragma mark - ScoreboardView

/**
 * @fn StackView *ScoreboardView::addColumn(ScoreboardView *self)
 * @memberof ScoreboardView
 */
static StackView *addColumn(ScoreboardView *self) {

  StackView *column = $(alloc(StackView), initWithFrame, NULL);
  assert(column);

  column->axis = StackViewAxisVertical;
  $((View *) column, addClassName, "column");

  $((View *) self->columns, addSubview, (View *) column);
  release(column);

  return column;
}

/**
 * @fn ScoreView *ScoreboardView::scoreView(ScoreboardView *self, const g_score_t *score)
 * @memberof ScoreboardView
 */
static ScoreView *scoreView(ScoreboardView *self, const g_score_t *score) {

  ScoreView *row = $(alloc(ScoreView), initWithScore, score, self->rowWidth);
  assert(row);

  configureScoreRow(row, score);

  return row;
}

/**
 * @fn void ScoreboardView::rebuild(ScoreboardView *self)
 * @memberof ScoreboardView
 */
static void rebuild(ScoreboardView *self) {

  $(self->title, setText, cgi.ConfigString(CS_MESSAGE));

  $((View *) self->header, removeAllSubviews);
  $((View *) self->columns, removeAllSubviews);

  size_t count;
  const g_score_t *scores = Cg_Scores(&count);

  if (count == 0) {
    return;
  }

  // The columns start 88 logical pixels down (scoreboard.css); the board used 64 for its title
  const size_t rows = rowsThatFit(self, 64);

  if (cg_state.num_teams) {

    // the aggregate scores follow the players' in the array
    const g_score_t *totals = scores + count;

    for (int32_t t = 0; t < cg_state.num_teams; t++) {
      Text *total = $(alloc(Text), initWithText, teamTotal(&cg_state.teams[t], &totals[t]), NULL);
      assert(total);

      const color32_t rgba = Color_Color32(cg_state.teams[t].color);
      total->color = (SDL_Color) { rgba.r, rgba.g, rgba.b, 255 };

      $((View *) self->header, addSubview, (View *) total);
      release(total);
    }

    StackView *spectators = $(self, addColumn);
    $((View *) spectators, addClassName, "spectators");

    for (int32_t t = 0; t < cg_state.num_teams; t++) {
      StackView *column = $(self, addColumn);

      size_t added = 0;
      for (size_t i = 0; i < count && added < rows; i++) {
        if (scores[i].team == t + 1) {
          addRow(self, column, &scores[i]);
          added++;
        }
      }
    }

    size_t added = 0;
    for (size_t i = 0; i < count && added < rows; i++) {
      if (scores[i].flags & SCORE_SPECTATOR) {
        addRow(self, spectators, &scores[i]);
        added++;
      }
    }

  } else {

    const size_t cols = count > rows ? 2 : 1;

    for (size_t c = 0; c < cols; c++) {
      StackView *column = $(self, addColumn);

      for (size_t i = c * rows; i < count && i < (c + 1) * rows; i++) {
        addRow(self, column, &scores[i]);
      }
    }
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((ScoreboardViewInterface *) clazz->interface)->addColumn = addColumn;
  ((ScoreboardViewInterface *) clazz->interface)->rebuild = rebuild;
  ((ScoreboardViewInterface *) clazz->interface)->scoreView = scoreView;
}

/**
 * @fn Class *ScoreboardView::_ScoreboardView(void)
 * @memberof ScoreboardView
 */
Class *_ScoreboardView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ScoreboardView",
      .superclass = _View(),
      .instanceSize = sizeof(ScoreboardView),
      .interfaceSize = sizeof(ScoreboardViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
