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
  release(this->title);

  super(Object, self, dealloc);
}

#pragma mark - Rows

/**
 * @brief Appends a row for `score` to `column`.
 */
static void addRow(ScoreboardView *self, StackView *column, const GameScore *score) {

  ScoreView *row = $(self, scoreView, score);
  assert(row);

  // every other row carries `odd`, so a table can stripe them
  if (((const Array *) column->view.subviews)->count & 1) {
    $((View *) row, addClassName, "odd");
  }

  $((View *) column, addSubview, (View *) row);
  release(row);
}

/**
 * @brief The badge over a player's icon: in CTF, the flag they carry.
 */
static void configureBadge(ScoreView *row, const GameScore *score) {
#if defined(G_CTF)
  if (score->flags & SCORE_CTF_FLAG) {
    $(row->badge, setImage, (Image *) Cg_HudImage(va("pics/flag%d", score->team)));
    $((View *) row->badge, setVisibility, ViewVisibilityVisible);
  }
#endif
}

/**
 * @brief The stock team total.
 */
static const char *teamTotal(const CGameTeamInfo *team, const GameScore *score) {
#if defined(G_CTF)
  return va("%s^7 %d captures", team->name, score->captures);
#else
  return va("%s^7 %d frags", team->name, score->frags);
#endif
}

/**
 * @brief The width a table row needs: the icon, a name, every field and the ping.
 */
static int32_t tableRowWidth(ScoreboardView *self) {

  const ScoreField *f;
  const size_t count = $(self, fields, &f);

  int32_t width = SCORES_ICON_WIDTH + SCORES_NAME_WIDTH + SCORES_PING_WIDTH;

  for (size_t i = 0; i < count; i++) {
    width += f[i].width;
  }

  return width;
}

/**
 * @brief The row of field captions a table's rows line up under.
 */
static View *tableHeader(ScoreboardView *self) {

  View *header = $(alloc(View), initWithFrame, &MakeRect(0, 0, self->rowWidth, SCORES_HEADER_HEIGHT));
  assert(header);

  $(header, addClassName, "fields");

  const ScoreField *f;
  const size_t count = $(self, fields, &f);

  // laid out from the right, exactly as ScoreView::setFields lays out the values
  int32_t x = self->rowWidth - SCORES_PING_WIDTH;

  for (size_t i = count; i > 0; i--) {

    x -= f[i - 1].width;

    View *column = $(alloc(View), initWithFrame, &MakeRect(x, 0, f[i - 1].width, SCORES_HEADER_HEIGHT));
    assert(column);

    Text *caption = $(alloc(Text), initWithText, f[i - 1].caption, NULL);
    assert(caption);

    $((View *) caption, addClassName, "caption");
    $(column, addSubview, (View *) caption);
    release(caption);

    $(header, addSubview, column);
    release(column);
  }

  return header;
}

/**
 * @brief A column of rows, leading with `total` if a team game gives one, then with the field
 * captions in the table layout.
 */
static StackView *addRowsColumn(ScoreboardView *self, View *total) {

  StackView *column = $(self, addColumn);

  if (total) {
    $((View *) column, addSubview, total);
  }

  if (self->layout == ScoreboardLayoutTable) {

    View *header = tableHeader(self);
    $((View *) column, addSubview, header);
    release(header);
  }

  return column;
}


/**
 * @brief The rows that fit beneath `top`, less whatever a layout reserves, at least three.
 */
static size_t rowsThatFit(const ScoreboardView *self, int32_t top, int32_t reserved) {
  return (size_t) Maxi(3, (self->view.frame.h - 2 * top - reserved) / SCORES_ROW_HEIGHT);
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

    this->columns = $(alloc(StackView), initWithFrame, NULL);
    assert(this->columns);

    $((View *) this->columns, addClassName, "columns");
    $(self, addSubview, (View *) this->columns);
  }

  return self;
}

static const EnumName layoutNames[] = MakeEnumNames(
  MakeEnumAlias(ScoreboardLayoutCards, cards),
  MakeEnumAlias(ScoreboardLayoutTable, table)
);

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

  super(View, self, awakeWithDictionary, dictionary);

  ScoreboardView *this = (ScoreboardView *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("layout", InletTypeEnum, &this->layout, (ident) layoutNames)
  );

  $(self, bind, inlets, dictionary);
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

  $((View *) column, addClassName, "rows");

  $((View *) self->columns, addSubview, (View *) column);
  release(column);

  return column;
}

/**
 * @fn ScoreView *ScoreboardView::scoreView(ScoreboardView *self, const GameScore *score)
 * @memberof ScoreboardView
 */
static ScoreView *scoreView(ScoreboardView *self, const GameScore *score) {

  ScoreView *row = $(alloc(ScoreView), initWithScore, score, self->rowWidth);
  assert(row);

  configureBadge(row, score);

  if (score->flags & SCORE_SPECTATOR) {
    $(row, setDetails, "spectating", NULL);
    return row;
  }

  if (self->layout == ScoreboardLayoutTable) {

    const ScoreField *fields;

    // the values are gathered into a fixed array, and a board with more fields than it holds
    // would put each value under the wrong caption
    const size_t count = $((ScoreboardView *) self, fields, &fields);
    assert(count <= SCORE_FIELDS_MAX);

    const char *values[SCORE_FIELDS_MAX];
    for (size_t i = 0; i < count; i++) {
      values[i] = $((ScoreboardView *) self, valueForField, score, i);
    }

    $(row, setFields, fields, values, count);

  } else {

    const char *detail = NULL, *aside = NULL;
    $((ScoreboardView *) self, describe, score, &detail, &aside);

    $(row, setDetails, detail, aside);
  }

  return row;
}

/**
 * @fn size_t ScoreboardView::fields(const ScoreboardView *self, const ScoreField **fields)
 * @memberof ScoreboardView
 */
static size_t fields(const ScoreboardView *self, const ScoreField **fields) {

  // lower case, as the prose reads it; the table's header uppercases it in the stylesheet
  static const ScoreField stock[] = {
#if defined(G_CTF)
    { "score", 64 },
#endif
    { "frags", 64 },
#if defined(G_CTF)
    { "captures", 80 },
    { "assists", 72 },
#endif
    { "deaths", 64 },
  };

  *fields = stock;
  return lengthof(stock);
}

/**
 * @fn const char *ScoreboardView::valueForField(const ScoreboardView *self, const GameScore *score, size_t field)
 * @memberof ScoreboardView
 */
static const char *valueForField(const ScoreboardView *self, const GameScore *score, size_t field) {

  switch (field) {
#if defined(G_CTF)
    case 0:
      return va("%d", score->score);
    case 1:
      return va("%d", score->frags);
    case 2:
      return va("%d", score->captures);
    case 3:
      return va("%d", score->assists);
    case 4:
#else
    case 0:
      return va("%d", score->frags);
    case 1:
#endif
      return va("%d", score->deaths);
    default:
      return "";
  }
}

/**
 * @fn void ScoreboardView::describe(const ScoreboardView *self, const GameScore *score, const char **detail, const char **aside)
 * @memberof ScoreboardView
 */
static void describe(const ScoreboardView *self, const GameScore *score, const char **detail, const char **aside) {

  const ScoreField *f;
  const size_t count = $((ScoreboardView *) self, fields, &f);

  // a card has two lines on each side of the name: the first half of the fields reads down the
  // left, and the rest down the right, beneath the ping
#if defined(G_CTF)
  // the score holds the frags, so a card leaves them to the table
  static const size_t shown[] = { 0, 2, 3, 4 };
#else
  static const size_t shown[] = { 0, 1 };
#endif

  char text[2][MAX_STRING_CHARS] = { "", "" };

  for (size_t i = 0; i < lengthof(shown) && shown[i] < count; i++) {
    char *side = text[i >= (lengthof(shown) + 1) / 2];
    const char *value = $((ScoreboardView *) self, valueForField, score, shown[i]);
    Str_Append(side, va("%s%s %s", *side ? "\n" : "", value, f[shown[i]].caption), MAX_STRING_CHARS);
  }

  *detail = *text[0] ? va("%s", text[0]) : NULL;
  *aside = *text[1] ? va("%s", text[1]) : NULL;
}

/**
 * @fn void ScoreboardView::rebuild(ScoreboardView *self)
 * @memberof ScoreboardView
 */
static void rebuild(ScoreboardView *self) {

  $(self->title, setText, cgi.ConfigString(CS_MESSAGE));

  $((View *) self->columns, removeAllSubviews);

  // layout treats an existing frame as authoritative, so a container that has been laid out
  // once never re-derives its size from new content: clear them, and they measure again
  $((View *) self->columns, resize, &MakeSize(0, 0));

  size_t count;
  const GameScore *scores = Cg_Scores(&count);

  if (count == 0) {
    return;
  }

  if (self->layout == ScoreboardLayoutTable) {
    self->rowWidth = tableRowWidth(self);
  }

  // The columns start 88 logical pixels down (scoreboard.css); the board used 64 for its title.
  // A team's column leads with its total, and a table's with its field captions, which take
  // room the rows then do not have
  const int32_t reserved = (self->layout == ScoreboardLayoutTable ? SCORES_HEADER_HEIGHT : 0) +
                           (cgameState.numTeams ? SCORES_TOTAL_HEIGHT : 0);
  const size_t rows = rowsThatFit(self, 64, reserved);

  if (cgameState.numTeams) {

    // the aggregate scores follow the players' in the array
    const GameScore *totals = scores + count;

    StackView *spectators = $(self, addColumn);
    $((View *) spectators, addClassName, "spectators");

    for (int32_t t = 0; t < cgameState.numTeams; t++) {
      Text *total = $(alloc(Text), initWithText, teamTotal(&cgameState.teams[t], &totals[t]), NULL);
      assert(total);

      $((View *) total, addClassName, "total");

      const Color32 rgba = Color_Color32(cgameState.teams[t].color);
      const SDL_Color color = { rgba.r, rgba.g, rgba.b, 255 };

      $(total->view.style, addColorAttribute, "color", &color);

      StackView *column = addRowsColumn(self, (View *) total);
      release(total);

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

    // a table reads as one list, however long it is
    const size_t cols = self->layout == ScoreboardLayoutTable ? 1 : (count > rows ? 2 : 1);

    for (size_t c = 0; c < cols; c++) {
      StackView *column = addRowsColumn(self, NULL);

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

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((ScoreboardViewInterface *) clazz->interface)->addColumn = addColumn;
  ((ScoreboardViewInterface *) clazz->interface)->describe = describe;
  ((ScoreboardViewInterface *) clazz->interface)->fields = fields;
  ((ScoreboardViewInterface *) clazz->interface)->rebuild = rebuild;
  ((ScoreboardViewInterface *) clazz->interface)->scoreView = scoreView;
  ((ScoreboardViewInterface *) clazz->interface)->valueForField = valueForField;
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
