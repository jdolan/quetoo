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


#include "ui_console.h"

#define CONSOLE_FONT_SIZE 16
#define CONSOLE_CURSOR "_"

int32_t Ui_ConsoleHeight(int32_t height) {
  return height * (cls.state == CL_ACTIVE ? Clampf01(cl_console_height->value) : 1.f);
}

#define _Class _ConsoleViewController

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
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  ConsoleViewController *this = (ConsoleViewController *) self;

  release(this->console);
  release(this->background);
  release(this->buffer);
  release(this->input);
  release(this->notify);
  release(this->chat);
  release(this->chatInput);

  super(Object, self, dealloc);
}

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  self->view->pointerEvents = false;

  ConsoleViewController *this = (ConsoleViewController *) self;

  this->console = $(alloc(View), initWithFrame, NULL);
  assert(this->console);

  this->console->autoresizingMask = ViewAutoresizingWidth;
  this->console->clipsSubviews = true;

  Image *conback = $$(Image, imageWithResourceName, "ui/conback.png");
  if (conback) {
    this->background = $(alloc(ImageView), initWithImage, conback);
    assert(this->background);

    this->background->view.alignment = ViewAlignmentInternal;

    $(this->console, addSubview, (View *) this->background);
    release(conback);
  }

  // The buffer sits in the padded bounds, above the input line, which escapes the padding
  this->buffer = addText(this->console, ViewAlignmentBottomLeft);
  this->input = addText(this->console, ViewAlignmentInternal);

  $(self->view, addSubview, this->console);

  this->notify = addText(self->view, ViewAlignmentTopLeft);

  this->chat = addText(self->view, ViewAlignmentNone);
  this->chatInput = addText(self->view, ViewAlignmentNone);
}

/**
 * @brief The character cell of the console font, in points.
 */
static SDL_Size cell(const Text *text) {
  return $(text, sizeText, "M");
}

/**
 * @return True once the Texts have a resolved Font and the layer has a size: before the first
 * render neither holds, and column counts derived from them would be garbage.
 */
static bool ready(const ConsoleViewController *self, const Text *text) {

  const SDL_Size ch = cell(text);
  const SDL_Rect frame = self->viewController.view->frame;

  return text->font && ch.w > 0 && ch.h > 0 && frame.w > 0 && frame.h > 0;
}

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
 * @brief The drop-down console: its height from `cl_console_height`, the buffer above the input.
 */
static void updateConsole(ConsoleViewController *self) {

  if (!ready(self, self->buffer)) {
    return;
  }

  const SDL_Size ch = cell(self->buffer);
  const SDL_Rect frame = self->viewController.view->frame;
  const int32_t height = Ui_ConsoleHeight(frame.h);

  cl_console.width = frame.w / ch.w;
  cl_console.height = Maxi(height / ch.h - 1, 0);

  if (self->console->frame.h != height) {
    $(self->console, resize, &MakeSize(self->console->frame.w, height));
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
  } else if (self->console->backgroundColor.a != alpha) {
    const SDL_Color color = { 0, 0, 0, alpha };
    self->console->backgroundColor = color;
    $(self->console->style, addColorAttribute, "background-color", &color);
  }

  if (self->console->padding.bottom != ch.h) {
    const SDL_Rect padding = MakeRect(0, 1, ch.h, 1);
    self->console->padding = MakePadding(0, 1, ch.h, 1);
    $(self->console->style, addRectangleAttribute, "padding", &padding);
    $(self->console, setNeedsLayout);
  }

  tail(&cl_console, cl_console.height, self->buffer);
  inputLine(&cl_console, ESC_COLOR_GREEN, self->input);

  self->input->view.frame.x = 1;
  self->input->view.frame.y = height - ch.h;
}

/**
 * @brief The last few lines of console output, for `cl_notify_time`.
 */
static void updateNotify(ConsoleViewController *self) {

  if (!ready(self, self->notify)) {
    return;
  }

  const SDL_Size ch = cell(self->notify);

  cl_notify_console.width = self->viewController.view->frame.w / ch.w;
  cl_notify_console.height = Clampf(cl_notify_lines->integer, 1, 12);
  cl_notify_console.level = (PRINT_MEDIUM | PRINT_HIGH);

  const uint32_t notify_millis = cl_notify_time->value * 1000;
  cl_notify_console.whence = Maxi(quetoo.ticks - notify_millis, cls.connect_time);

  tail(&cl_notify_console, cl_notify_console.height, self->notify);
}

/**
 * @brief Recent chat, and the chat input while typing.
 */
static void updateChat(ConsoleViewController *self, bool typing) {

  if (!ready(self, self->chat)) {
    return;
  }

  const SDL_Size ch = cell(self->chat);
  const SDL_Rect frame = self->viewController.view->frame;

  cl_chat_console.width = frame.w / ch.w / 3;
  cl_chat_console.height = Clampf(cl_chat_lines->integer, 0, 16);

  const bool history = cl_draw_chat->value && cl_chat_console.height;

  $((View *) self->chat, setHidden, !history);

  int32_t y = frame.h * 0.66;

  if (history) {
    if (typing) {
      cl_chat_console.whence = cls.connect_time;
    } else if (quetoo.ticks > cl_chat_time->value * 1000) {
      cl_chat_console.whence = quetoo.ticks - cl_chat_time->value * 1000;
    }

    tail(&cl_chat_console, cl_chat_console.height, self->chat);

    self->chat->view.frame.x = 0;
    self->chat->view.frame.y = y;

    y += self->chat->view.frame.h;
  }

  $((View *) self->chatInput, setHidden, !typing);

  if (typing) {
    inputLine(&cl_chat_console, cls.chat_state.team_chat ? ESC_COLOR_TEAM_CHAT : ESC_COLOR_CHAT, self->chatInput);

    self->chatInput->view.frame.x = 0;
    self->chatInput->view.frame.y = y;
  }
}

/**
 * @fn void ConsoleViewController::update(ConsoleViewController *self)
 * @memberof ConsoleViewController
 */
static void update(ConsoleViewController *self) {

  const cl_key_dest_t dest = cls.key_state.dest;
  const bool active = cls.state == CL_ACTIVE;

  const bool console = dest == KEY_CONSOLE && cls.state != CL_LOADING;
  const bool notify = active && dest == KEY_GAME && cl_draw_notify->value;
  const bool chat = active && (dest == KEY_GAME || dest == KEY_CHAT);

  $(self->console, setHidden, !console);
  if (console) {
    updateConsole(self);
  }

  $((View *) self->notify, setHidden, !notify);
  if (notify) {
    updateNotify(self);
  }

  if (chat) {
    updateChat(self, dest == KEY_CHAT);
  } else {
    $((View *) self->chat, setHidden, true);
    $((View *) self->chatInput, setHidden, true);
  }
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;

  ((ConsoleViewControllerInterface *) clazz->interface)->update = update;
}

/**
 * @fn Class *ConsoleViewController::_ConsoleViewController(void)
 * @memberof ConsoleViewController
 */
Class *_ConsoleViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ConsoleViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(ConsoleViewController),
      .interfaceSize = sizeof(ConsoleViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
