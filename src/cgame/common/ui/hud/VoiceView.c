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
#include "VoiceView.h"

#define _Class _VoiceView

/**
 * @brief How long after a client's last frame they stop being shown as speaking.
 * @details Comfortably longer than the 20ms frame interval, so a pause between words does not
 * blink the indicator, and short enough that it clears promptly when someone lets go of the key.
 */
#define VOICE_VIEW_TIMEOUT 300

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = (View *) super(StackView, self, initWithFrame, NULL);
  if (self) {

    VoiceView *this = (VoiceView *) self;

    this->icon = $(alloc(ImageView), initWithFrame, NULL);
    assert(this->icon);

    $(this->icon, setImage, (Image *) Cg_HudImage("pics/voice"));
    $((View *) this->icon, addClassName, "icon");

    $(self, addSubview, (View *) this->icon);

    this->names = $(alloc(Text), initWithText, "", NULL);
    assert(this->names);

    $((View *) this->names, addClassName, "names");

    $(self, addSubview, (View *) this->names);

    $(self, setVisibility, ViewVisibilityHidden);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  VoiceView *this = (VoiceView *) self;

  char names[MAX_STRING_CHARS];
  names[0] = '\0';

  for (int32_t i = 0; i < MAX_CLIENTS; i++) {

    const uint32_t time = cgi.client->voice_time[i];

    if (!time || cgi.client->unclamped_time - time > VOICE_VIEW_TIMEOUT) {
      continue;
    }

    if (names[0]) {
      q_strlcat(names, ", ", sizeof(names));
    }

    q_strlcat(names, cg_state.clients[i].name, sizeof(names));
  }

  if (names[0]) {
    $(this->names, setText, names);
    $(self, setVisibility, ViewVisibilityVisible);
  } else {
    $(self, setVisibility, ViewVisibilityHidden);
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *VoiceView::_VoiceView(void)
 * @memberof VoiceView
 */
Class *_VoiceView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "VoiceView",
      .superclass = _StackView(),
      .instanceSize = sizeof(VoiceView),
      .interfaceSize = sizeof(VoiceViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
