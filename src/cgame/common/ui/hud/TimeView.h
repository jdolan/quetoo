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

#include "CounterView.h"

/**
 * @file
 * @brief The match clock, from the `CS_TIME` config string.
 */

typedef struct TimeView TimeView;
typedef struct TimeViewInterface TimeViewInterface;

/**
 * @brief The match clock, from the `CS_TIME` config string, captioned like the counters
 * beside it.
 * @remarks The server leads the string with `^7` for neutral, which would defeat the
 * stylesheet's color, so that one escape is dropped; the `^2` countdown flash is kept.
 * @extends CounterView
 */
struct TimeView {

  /**
   * @brief The superclass.
   */
  CounterView counterView;

  /**
   * @brief The interface type.
   * @protected
   */
  TimeViewInterface *interface[0];
};

struct TimeViewInterface {

  /**
   * @brief The superclass interface.
   */
  CounterViewInterface counterViewInterface;
};

CGAME_EXPORT Class *_TimeView(void);
