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

#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/Text.h>

/**
 * @file
 * @brief The seconds until a dead player may respawn.
 */

typedef struct RespawnTimerView RespawnTimerView;
typedef struct RespawnTimerViewInterface RespawnTimerViewInterface;

/**
 * @brief The seconds until a dead player may respawn, after a delay of more than 2 seconds
 * (`STAT_RESPAWN`). Hidden otherwise.
 * @extends StackView
 */
struct RespawnTimerView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  RespawnTimerViewInterface *interface[0];

  /**
   * @brief The seconds shown, or 0.
   */
  int16_t seconds;

  /**
   * @brief The countdown.
   */
  Text *text;
};

struct RespawnTimerViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;
};

CGAME_EXPORT Class *_RespawnTimerView(void);
