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

#include <ObjectivelyMVC/View.h>

/**
 * @file
 * @brief A translucent band in the player's team colour.
 */

typedef struct TeamBannerView TeamBannerView;
typedef struct TeamBannerViewInterface TeamBannerViewInterface;

/**
 * @brief A translucent band in the player's team colour. Hidden when not on a team.
 * @extends View
 */
struct TeamBannerView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  TeamBannerViewInterface *interface[0];
};

struct TeamBannerViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;
};

CGAME_EXPORT Class *_TeamBannerView(void);
