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

#include <ObjectivelyMVC/Text.h>

/**
 * @file
 * @brief A Text showing the tail of the console, filtered.
 */

typedef struct ConsoleText ConsoleText;
typedef struct ConsoleTextInterface ConsoleTextInterface;

/**
 * @brief A monospace Text showing the most recent console lines that pass its filter, wrapped
 * to a width in points. The notify lines and the chat history are both one of these.
 * @extends Text
 */
struct ConsoleText {

  /**
   * @brief The superclass.
   */
  Text text;

  /**
   * @brief The interface type.
   * @protected
   */
  ConsoleTextInterface *interface[0];

  /**
   * @brief The filter: `level` selects prints, `whence` the oldest to show.
   */
  console_t console;
};

struct ConsoleTextInterface {

  /**
   * @brief The superclass interface.
   */
  TextInterface textInterface;

  /**
   * @fn void ConsoleText::tail(ConsoleText *self, int32_t width, size_t lines)
   * @brief Shows the last `lines` lines passing the filter, wrapped to `width` points.
   * @param self The ConsoleText.
   * @param width The width to wrap to, in points.
   * @param lines The count of lines to show; `0` clears the text.
   * @remarks Does nothing until the font has resolved, since the column count derives from it.
   * @memberof ConsoleText
   */
  void (*tail)(ConsoleText *self, int32_t width, size_t lines);
};

CGAME_EXPORT Class *_ConsoleText(void);
