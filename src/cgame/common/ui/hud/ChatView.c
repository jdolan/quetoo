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

#include "ChatView.h"

#define _Class _ChatView

#define CHAT_MAX_LINES 16

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  ChatView *this = (ChatView *) self;

  release(this->history);
  release(this->input);

  super(Object, self, dealloc);
}

#pragma mark - TextViewDelegate

/**
 * @brief Sends the line when editing ended on Enter, and discards it when the client already
 * left chat, as it does on Escape.
 */
static void didEndEditing(TextView *textView) {

  if (cgi.GetKeyDest() != KEY_CHAT) {
    return;
  }

  const char *line = textView->attributedText->chars;
  if (*line) {
    const SDL_Keymod mods = SDL_GetModState();
    const bool team = cg_hud_state.chat.team || (mods & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL));

    char command[MAX_PRINT_MSG];
    q_snprintf(command, sizeof(command), "%s %.*s^7\n", team ? "say_team" : "say", MAX_PRINT_MSG - 32, line);

    cgi.Cbuf(command);
  }

  cgi.SetKeyDest(KEY_GAME);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = (View *) super(StackView, self, initWithFrame, NULL);
  if (self) {
    ChatView *this = (ChatView *) self;

    this->stackView.axis = StackViewAxisVertical;
    self->autoresizingMask = ViewAutoresizingContain;

    this->history = (ConsoleText *) $((View *) alloc(ConsoleText), init);
    assert(this->history);

    this->history->console.level = PRINT_CHAT | PRINT_TEAM_CHAT;

    $(self, addSubview, (View *) this->history);

    this->input = $(alloc(TextView), initWithFrame, NULL);
    assert(this->input);

    this->input->delegate.self = this;
    this->input->delegate.didEndEditing = didEndEditing;

    $((View *) this->input, setHidden, true);

    $(self, addSubview, (View *) this->input);
  }

  return self;
}

/**
 * @brief Opens the input for a new line, styled for the chat requested.
 */
static void beginTyping(ChatView *self) {

  TextView *input = self->input;
  View *view = (View *) input;

  $(input, setAttributedText, "");
  input->position = 0;

  if (cg_hud_state.chat.team) {
    $(view, addClassName, "team");
    $(input, setDefaultText, "say_team");
  } else {
    $(view, removeClassName, "team");
    $(input, setDefaultText, "say");
  }

  $(view, becomeKeyResponder);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  ChatView *this = (ChatView *) self;

  const bool typing = cgi.GetKeyDest() == KEY_CHAT;

  if (typing && !this->typing) {
    beginTyping(this);
  } else if (!typing && this->typing) {
    if ($((Control *) this->input, isFocused)) {
      $((View *) this->input, resignKeyResponder);
    }
  }

  this->typing = typing;

  $((View *) this->input, setHidden, !typing);

  const size_t lines = Clampf(cg_chat_lines->integer, 0, CHAT_MAX_LINES);

  $((View *) this->history, setHidden, lines == 0);

  if (data && lines && self->superview) {
    const uint32_t now = (uint32_t) SDL_GetTicks();
    const uint32_t millis = cg_chat_time->value * 1000;

    const uint32_t since = typing || now < millis ? 0 : now - millis;

    this->history->console.whence = since > cg_hud_state.clear_time ? since : cg_hud_state.clear_time;

    $(this->history, tail, self->superview->frame.w / 3, lines);
  }

  super(View, self, updateBindings, data);
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *ChatView::_ChatView(void)
 * @memberof ChatView
 */
Class *_ChatView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ChatView",
      .superclass = _StackView(),
      .instanceSize = sizeof(ChatView),
      .interfaceSize = sizeof(ChatViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
