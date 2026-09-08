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
 * @brief The round trip time to the server.
 */

typedef struct PingView PingView;
typedef struct PingViewInterface PingViewInterface;

/**
 * @brief The round trip time to the server, in milliseconds, as the client measures it.
 * @details The value carries the class name `lagging` while packets were dropped in the last
 * second or the round trip exceeds `cg_draw_ping_warn`. Hidden when `cg_draw_ping` is off.
 * @extends CounterView
 */
struct PingView {

  /**
   * @brief The superclass.
   */
  CounterView counterView;

  /**
   * @brief The interface type.
   * @protected
   */
  PingViewInterface *interface[0];

  /**
   * @brief The dropped packet count last seen, and when it last grew.
   */
  uint32_t dropped, dropped_time;
};

struct PingViewInterface {

  /**
   * @brief The superclass interface.
   */
  CounterViewInterface counterViewInterface;
};

CGAME_EXPORT Class *_PingView(void);
