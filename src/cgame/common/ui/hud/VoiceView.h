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

#pragma once

#include <ObjectivelyMVC.h>

/**
 * @file
 * @brief Shows who is currently speaking.
 */

typedef struct VoiceView VoiceView;
typedef struct VoiceViewInterface VoiceViewInterface;

/**
 * @brief A speaker icon and the names of everyone currently being heard.
 * @details Voice frames arrive at 50 per second, so a speaker is considered to have stopped only
 * after a short silence, which keeps the indicator from flickering between words.
 * @extends StackView
 */
struct VoiceView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  VoiceViewInterface *interface[0];

  /**
   * @brief The speaker icon.
   */
  ImageView *icon;

  /**
   * @brief The speakers' names.
   */
  Text *names;
};

struct VoiceViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;
};

/**
 * @fn Class *VoiceView::_VoiceView(void)
 * @memberof VoiceView
 */
OBJECTIVELYMVC_EXPORT Class *_VoiceView(void);
