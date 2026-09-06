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
#include "ScoreRowView.h"

#define _Class _ScoreRowView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  ScoreRowView *this = (ScoreRowView *) self;

  release(this->aside);
  release(this->badge);
  release(this->detail);
  release(this->fill);
  release(this->icon);
  release(this->name);
  release(this->ping);

  super(Object, self, dealloc);
}

#pragma mark - ScoreRowView

/**
 * @brief Adds a Text child at the given frame, with the given class name.
 */
static Text *addText(ScoreRowView *self, const SDL_Rect *frame, const char *className) {

  Text *text = $(alloc(Text), initWithText, NULL, NULL);
  assert(text);

  text->view.frame = *frame;

  $((View *) text, addClassName, className);
  $((View *) self, addSubview, (View *) text);

  return text;
}

/**
 * @fn ScoreRowView *ScoreRowView::initWithScore(ScoreRowView *self, const g_score_t *score, int32_t width)
 * @memberof ScoreRowView
 */
static ScoreRowView *initWithScore(ScoreRowView *self, const g_score_t *score, int32_t width) {

  self = (ScoreRowView *) super(View, self, initWithFrame, &MakeRect(0, 0, width, SCORES_ROW_HEIGHT));
  if (self) {

    const cg_client_info_t *info = &cg_state.clients[score->client];

    self->icon = $(alloc(ImageView), initWithFrame, &MakeRect(1, 1, SCORES_ICON_WIDTH - 2, SCORES_ICON_WIDTH - 2));
    assert(self->icon);

    $(self->icon, setImage, (Image *) Cg_HudImage(va("players/%s/%s_i", info->model, info->skin)));
    $((View *) self, addSubview, (View *) self->icon);

    self->badge = $(alloc(ImageView), initWithFrame, &MakeRect(1, 1, SCORES_ICON_WIDTH * 0.3f, SCORES_ICON_WIDTH * 0.3f));
    assert(self->badge);

    $((View *) self->badge, setHidden, true);
    $((View *) self, addSubview, (View *) self->badge);

    const int32_t x = SCORES_ICON_WIDTH;
    const int32_t fw = width - SCORES_ICON_WIDTH - 1;

    self->fill = $(alloc(View), initWithFrame, &MakeRect(x, 0, fw, SCORES_ROW_HEIGHT - 1));
    assert(self->fill);

    $((View *) self->fill, addClassName, "fill");
    $((View *) self, addSubview, self->fill);

    if (score->color >= 0) {
      color_t c = ColorHSV(score->color, 1.f, 1.f);
      c.a = score->client == cgi.client->frame.ps.client ? .3f : .15f;

      const color32_t rgba = Color_Color32(c);
      self->fill->backgroundColor = (SDL_Color) { rgba.r, rgba.g, rgba.b, rgba.a };
    }

    self->name = addText(self, &MakeRect(x, 0, fw, 0), "name");
    $(self->name, setText, info->name);

    self->ping = addText(self, &MakeRect(x, 0, fw, 0), "ping");
    self->ping->view.alignment = ViewAlignmentTopRight;
    $(self->ping, setTextWithFormat, "%3dms", score->ping);

    self->detail = addText(self, &MakeRect(x, 16, fw, 0), "detail");

    self->aside = addText(self, &MakeRect(x, 16, fw, 0), "aside");
    self->aside->view.alignment = ViewAlignmentRight;
  }

  return self;
}

/**
 * @fn void ScoreRowView::setDetails(ScoreRowView *self, const char *detail, const char *aside)
 * @memberof ScoreRowView
 */
static void setDetails(ScoreRowView *self, const char *detail, const char *aside) {

  $(self->detail, setText, detail);
  $(self->aside, setText, aside);

  $((View *) self->detail, setHidden, detail == NULL);
  $((View *) self->aside, setHidden, aside == NULL);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ScoreRowViewInterface *) clazz->interface)->initWithScore = initWithScore;
  ((ScoreRowViewInterface *) clazz->interface)->setDetails = setDetails;
}

/**
 * @fn Class *ScoreRowView::_ScoreRowView(void)
 * @memberof ScoreRowView
 */
Class *_ScoreRowView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ScoreRowView",
      .superclass = _View(),
      .instanceSize = sizeof(ScoreRowView),
      .interfaceSize = sizeof(ScoreRowViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
