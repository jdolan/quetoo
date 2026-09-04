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

#include <ObjectivelyMVC/ImageView.h>
#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/Text.h>

/**
 * @file
 * @brief A vital sign: a value beside its icon, coloured by thresholds.
 */

typedef enum {
  StatViewHealth,
  StatViewArmor,
  StatViewAmmo
} StatViewStat;

typedef struct StatView StatView;
typedef struct StatViewInterface StatViewInterface;

/**
 * @brief A vital sign: a value beside its icon, coloured by thresholds.
 * @details Configured in JSON by `stat`: `health`, `armor` or `ammo`. Hidden when the value
 * is zero, and for ammo in instagib. The icon pulses when the value is low and
 * `cg_draw_vitals_pulse` is set.
 * @extends StackView
 */
struct StatView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  StatViewInterface *interface[0];

  /**
   * @brief The icon.
   */
  ImageView *icon;

  /**
   * @brief The resource name of the icon shown, so it is re-resolved only on change.
   */
  const char *iconName;

  /**
   * @brief The vital shown.
   */
  StatViewStat stat;

  /**
   * @brief The value.
   */
  Text *value;
};

struct StatViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;

  /**
   * @fn StatView *StatView::initWithStat(StatView *self, StatViewStat stat)
   * @brief Initializes this StatView for the given vital.
   * @param self The StatView.
   * @param stat The vital.
   * @return The initialized StatView, or `NULL` on error.
   * @memberof StatView
   */
  StatView *(*initWithStat)(StatView *self, StatViewStat stat);
};

CGAME_EXPORT Class *_StatView(void);
