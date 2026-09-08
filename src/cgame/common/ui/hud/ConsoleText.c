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

#include "ConsoleText.h"

#define _Class _ConsoleText

#define CONSOLE_TEXT_MAX_LINES 32
#define CONSOLE_TEXT_INTERVAL 100

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = (View *) super(Text, self, initWithText, NULL, NULL);
  if (self) {
    $(self, addClassName, "live");
  }

  return self;
}

#pragma mark - ConsoleText

/**
 * @fn void ConsoleText::tail(ConsoleText *self, int32_t width, size_t lines)
 * @memberof ConsoleText
 */
static void tail(ConsoleText *self, int32_t width, size_t lines) {

  Text *text = (Text *) self;

  if (lines == 0) {
    $(text, setText, NULL);
    return;
  }

  const uint32_t now = (uint32_t) SDL_GetTicks();
  if (now - self->time < CONSOLE_TEXT_INTERVAL) {
    return;
  }

  const SDL_Size cell = $(text, sizeText, "M");
  if (text->font == NULL || cell.w <= 0) {
    return;
  }

  self->time = now;

  self->console.width = Maxi(width / cell.w, 1);
  self->console.height = lines = Mini(lines, CONSOLE_TEXT_MAX_LINES);

  char *strings[CONSOLE_TEXT_MAX_LINES];
  const size_t count = cgi.Tail(&self->console, strings, lines);

  size_t size = 1;
  for (size_t i = 0; i < count; i++) {
    size += strlen(strings[i]) + 1;
  }

  char *joined = cgi.Malloc(size, MEM_TAG_UI);
  char *c = joined;

  for (size_t i = 0; i < count; i++) {
    if (i) {
      *c++ = '\n';
    }
    const size_t len = strlen(strings[i]);
    memcpy(c, strings[i], len);
    c += len;
    cgi.Free(strings[i]);
  }
  *c = '\0';

  $(text, setText, joined);
  cgi.Free(joined);
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;

  ((ConsoleTextInterface *) clazz->interface)->tail = tail;
}

/**
 * @fn Class *ConsoleText::_ConsoleText(void)
 * @memberof ConsoleText
 */
Class *_ConsoleText(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ConsoleText",
      .superclass = _Text(),
      .instanceSize = sizeof(ConsoleText),
      .interfaceSize = sizeof(ConsoleTextInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
