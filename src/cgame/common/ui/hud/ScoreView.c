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
#include "ScoreView.h"

#define _Class _ScoreView

#pragma mark - ScoreView

/**
 * @fn ScoreView *ScoreView::initWithScore(ScoreView *self, const GameScore *score, int32_t width)
 * @memberof ScoreView
 */
static ScoreView *initWithScore(ScoreView *self, const GameScore *score, int32_t width) {

  self = (ScoreView *) super(View, self, initWithFrame, &MakeRect(0, 0, width, SCORES_ROW_HEIGHT));
  if (self) {

    Outlet outlets[] = MakeOutlets(
      MakeOutlet("icon", &self->icon),
      MakeOutlet("badge", &self->badge),
      MakeOutlet("fill", &self->fill),
      MakeOutlet("name", &self->name),
      MakeOutlet("ping", &self->ping),
      MakeOutlet("detail", &self->detail),
      MakeOutlet("aside", &self->aside)
    );

    View *this = (View *) self;

    $(this, awakeWithResourceName, "ui/hud/ScoreView.json");
    $(this, resolve, outlets);

    // a row is its width, whatever its children reach: the ping is aligned rather than
    // placed, so a size derived from the children alone falls short of the columns
    $(self->view.style, addIntegerAttribute, "min-width", width);

    const ClientGameClientInfo *info = &cgState.clients[score->client];

    if (score->client == cgi.client->frame.ps.client) {
      $(this, addClassName, "self");
    }

    self->icon->view.frame = MakeRect(1, 1, SCORES_ICON_WIDTH - 2, SCORES_ICON_WIDTH - 2);
    $(self->icon, setImage, (Image *) Cg_HudImage(va("players/%s/%s_i", info->model, info->skin)));

    self->badge->view.frame = MakeRect(1, 1, SCORES_ICON_WIDTH * 0.3f, SCORES_ICON_WIDTH * 0.3f);
    $((View *) self->badge, setVisibility, ViewVisibilityHidden);

    const int32_t x = SCORES_ICON_WIDTH;
    const int32_t fw = width - SCORES_ICON_WIDTH - 1;

    self->fill->frame = MakeRect(x, 0, fw, SCORES_ROW_HEIGHT - 1);

    if (score->color >= 0) {
      Color c = ColorHSV(score->color, 1.f, 1.f);
      c.a = score->client == cgi.client->frame.ps.client ? .3f : .15f;

      const Color32 rgba = Color_Color32(c);
      const SDL_Color fill = { rgba.r, rgba.g, rgba.b, rgba.a };

      $(self->fill->style, addColorAttribute, "background-color", &fill);
    }

    self->name->view.frame = MakeRect(x, 0, fw, 0);
    $(self->name, setText, info->name);

    self->ping->view.frame = MakeRect(x, 0, fw, 0);
    $(self->ping, setTextWithFormat, "%dms", score->ping);

    self->detail->view.frame = MakeRect(x, 16, fw, 0);

    self->aside->view.frame = MakeRect(x, 16, fw, 0);
  }

  return self;
}

/**
 * @fn void ScoreView::setFields(ScoreView *self, const ScoreField *fields, const char **values, size_t count)
 * @memberof ScoreView
 */
static void setFields(ScoreView *self, const ScoreField *fields, const char **values, size_t count) {

  assert(fields);
  assert(values);

  $((View *) self, addClassName, "fields");

  $((View *) self->detail, setVisibility, ViewVisibilityHidden);
  $((View *) self->aside, setVisibility, ViewVisibilityHidden);

  const int32_t height = self->view.frame.h;

  // measured from the right, past the ping, so the columns meet the header above them
  int32_t x = self->view.frame.w - SCORES_PING_WIDTH;

  for (size_t i = count; i > 0; i--) {

    const ScoreField *field = &fields[i - 1];

    x -= field->width;

    // the value is aligned inside a column of its own: alignment resolves against the
    // superview, so it would otherwise discard the offset measured for it here
    View *column = $(alloc(View), initWithFrame, &MakeRect(x, 0, field->width, height));
    assert(column);

    Text *value = $(alloc(Text), initWithText, values[i - 1], NULL);
    assert(value);

    $((View *) value, addClassName, "field");
    $(column, addSubview, (View *) value);
    release(value);

    $((View *) self, addSubview, column);
    release(column);
  }

  // the prose lines are gone, so the name and the ping take the middle of the row
  self->name->view.frame.y = (height - self->name->view.frame.h) / 2;
}

/**
 * @fn void ScoreView::setDetails(ScoreView *self, const char *detail, const char *aside)
 * @memberof ScoreView
 */
static void setDetails(ScoreView *self, const char *detail, const char *aside) {

  $(self->detail, setText, detail);
  $(self->aside, setText, aside);

  $((View *) self->detail, setVisibility,
    detail == NULL ? ViewVisibilityHidden : ViewVisibilityVisible);
  $((View *) self->aside, setVisibility,
    aside == NULL ? ViewVisibilityHidden : ViewVisibilityVisible);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ScoreViewInterface *) clazz->interface)->initWithScore = initWithScore;
  ((ScoreViewInterface *) clazz->interface)->setDetails = setDetails;
  ((ScoreViewInterface *) clazz->interface)->setFields = setFields;
}

/**
 * @fn Class *ScoreView::_ScoreView(void)
 * @memberof ScoreView
 */
Class *_ScoreView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ScoreView",
      .superclass = _View(),
      .instanceSize = sizeof(ScoreView),
      .interfaceSize = sizeof(ScoreViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
