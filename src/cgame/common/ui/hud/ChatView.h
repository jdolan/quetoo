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

#pragma once

#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/TextView.h>

#include "ConsoleText.h"

/**
 * @file
 * @brief Recent chat, and the chat input while typing.
 */

typedef struct ChatView ChatView;
typedef struct ChatViewInterface ChatViewInterface;

/**
 * @brief Recent chat above the chat input.
 * @details The history shows `cg_chat_lines` lines for `cg_chat_time` seconds, or all of them
 * while typing. Typing begins when the key destination becomes `KEY_CHAT`, from
 * `cg_message_mode` or `cg_message_mode_2`, and ends when it leaves: Enter sends the line as
 * `say`, or `say_team` for team chat or with Shift or Ctrl held, and Escape discards it.
 * @extends StackView
 */
struct ChatView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  ChatViewInterface *interface[0];

  /**
   * @brief The recent chat.
   */
  ConsoleText *history;

  /**
   * @brief The input line, focused while typing.
   */
  TextView *input;

  /**
   * @brief True while the key destination is `KEY_CHAT`.
   */
  bool typing;
};

struct ChatViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;
};

CGAME_EXPORT Class *_ChatView(void);
