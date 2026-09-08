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

#include "ui_local.h"
#include "cl_local.h"

#include "ConsoleView.h"

#define _Class _ConsoleView

#define CONSOLE_FONT_SIZE 16
#define CONSOLE_CURSOR "_"

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  ConsoleView *this = (ConsoleView *) self;

  release(this->background);
  release(this->buffer);
  release(this->input);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @brief A monospace Text, since all of this changes every frame and wraps by column.
 */
static Text *addText(View *view, ViewAlignment alignment) {

  Text *text = $(alloc(Text), initWithText, NULL, NULL);
  assert(text);

  text->view.alignment = alignment;

  $(text->view.style, addCharactersAttribute, "font-family", DEFAULT_MONOSPACE_FONT_FAMILY);
  $(text->view.style, addIntegerAttribute, "font-size", CONSOLE_FONT_SIZE);

  $(view, addSubview, (View *) text);

  return text;
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = super(View, self, initWithFrame, NULL);
  if (self) {
    ConsoleView *this = (ConsoleView *) self;

    self->autoresizingMask = ViewAutoresizingWidth;
    self->clipsSubviews = true;

    Image *conback = $$(Image, imageWithResourceName, "ui/conback.png");
    if (conback) {
      this->background = $(alloc(ImageView), initWithImage, conback);
      assert(this->background);

      this->background->view.alignment = ViewAlignmentInternal;

      $(self, addSubview, (View *) this->background);
      release(conback);
    }

    // The buffer sits in the padded bounds, above the input line, which escapes the padding
    this->buffer = addText(self, ViewAlignmentBottomLeft);
    this->input = addText(self, ViewAlignmentInternal);
  }

  return self;
}

#pragma mark - ConsoleView

/**
 * @brief Joins the tail of `console` into `text`. Con_Wrap opens each line in its own color, so
 * nothing carries between them.
 */
static void tail(const console_t *console, size_t height, Text *text) {

  if (height == 0) {
    $(text, setText, NULL);
    return;
  }

  char *lines[height];
  const size_t count = Con_Tail(console, lines, height);

  size_t size = 1;
  for (size_t i = 0; i < count; i++) {
    size += strlen(lines[i]) + 1;
  }

  char *joined = Mem_Malloc(size);
  char *c = joined;

  for (size_t i = 0; i < count; i++) {
    if (i) {
      *c++ = '\n';
    }
    const size_t len = strlen(lines[i]);
    memcpy(c, lines[i], len);
    c += len;
    Mem_Free(lines[i]);
  }
  *c = '\0';

  $(text, setText, joined);
  Mem_Free(joined);
}

/**
 * @brief Copies `s` into `out`, doubling carets so Text draws them rather than reading escapes.
 */
static size_t escapeCarets(const char *s, size_t count, char *out, size_t size) {

  size_t len = 0;
  for (size_t i = 0; i < count && s[i] && len + 2 < size; i++) {
    if (s[i] == ESC_COLOR) {
      out[len++] = ESC_COLOR;
    }
    out[len++] = s[i];
  }
  out[len] = '\0';

  return len;
}

/**
 * @brief The input line: the prompt in `esc`, the buffer scrolled to keep the cursor in view,
 * and the cursor at the insertion point. Typed carets are literal, not color escapes.
 */
static void inputLine(const console_t *console, int32_t esc, Text *text) {

  const char *s = console->input.buffer;
  size_t pos = console->input.pos;

  if (pos > console->width - 2) {
    const size_t skip = 2 + pos - console->width;
    s += skip;
    pos -= skip;
  }

  const size_t at = min(pos, strlen(s));

  char before[MAX_PRINT_MSG * 2], after[MAX_PRINT_MSG * 2];
  escapeCarets(s, at, before, sizeof(before));
  escapeCarets(s + at, SIZE_MAX, after, sizeof(after));

  $(text, setTextWithFormat, "^%d]^7%s" CONSOLE_CURSOR "%s", esc, before, after);
}

/**
 * @fn void ConsoleView::update(ConsoleView *self, int32_t height)
 * @memberof ConsoleView
 */
static void update(ConsoleView *self, int32_t height) {

  View *view = (View *) self;

  if (view->superview == NULL || self->buffer->font == NULL) {
    return;
  }

  const SDL_Size ch = $(self->buffer, sizeText, "M");
  const SDL_Rect frame = view->superview->frame;

  if (ch.w <= 0 || ch.h <= 0 || frame.w <= 0 || frame.h <= 0) {
    return;
  }

  cl_console.width = Maxi(frame.w / ch.w, 2);
  cl_console.height = Maxi(height / ch.h - 1, 0);

  if (view->frame.h != height) {
    $(view, resize, &MakeSize(view->frame.w, height));
  }

  // Written directly for this frame, and into the element style so a theme reapply keeps them
  const Uint8 alpha = (Uint8) (Clampf01(cl_draw_console_background_alpha->value) * 255);

  if (self->background) {
    self->background->color.a = alpha;

    // Cover the screen, keeping the aspect, with the art's bottom edge on the console's
    const SDL_Size image = $(self->background->image, size);
    const float scale = Maxf(frame.w / (float) image.w, frame.h / (float) image.h);
    const SDL_Rect art = MakeRect(
      (frame.w - image.w * scale) / 2.f, height - image.h * scale, image.w * scale, image.h * scale
    );

    if (!SDL_RectsEqual(&art, &self->background->view.frame)) {
      self->background->view.frame = art;
    }
  } else if (view->backgroundColor.a != alpha) {
    const SDL_Color color = { 0, 0, 0, alpha };
    view->backgroundColor = color;
    $(view->style, addColorAttribute, "background-color", &color);
  }

  if (view->padding.bottom != ch.h) {
    const SDL_Rect padding = MakeRect(0, 1, ch.h, 1);
    view->padding = MakePadding(0, 1, ch.h, 1);
    $(view->style, addRectangleAttribute, "padding", &padding);
    $(view, setNeedsLayout);
  }

  tail(&cl_console, cl_console.height, self->buffer);
  inputLine(&cl_console, ESC_COLOR_GREEN, self->input);

  self->input->view.frame.x = 1;
  self->input->view.frame.y = height - ch.h;
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;

  ((ConsoleViewInterface *) clazz->interface)->update = update;
}

/**
 * @fn Class *ConsoleView::_ConsoleView(void)
 * @memberof ConsoleView
 */
Class *_ConsoleView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ConsoleView",
      .superclass = _View(),
      .instanceSize = sizeof(ConsoleView),
      .interfaceSize = sizeof(ConsoleViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
