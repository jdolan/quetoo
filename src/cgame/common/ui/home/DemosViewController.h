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

#include "cg_types.h"

#include <ObjectivelyMVC.h>

#include "DemosCollectionView.h"

/**
 * @file
 * @brief Demos ViewController: browse, filter and play recorded demos. Starring and deletion
 * are handled per-item, by DemosCollectionItemView.
 */

typedef struct DemosViewController DemosViewController;
typedef struct DemosViewControllerInterface DemosViewControllerInterface;

/**
 * @brief The DemosViewController type.
 * @extends ViewController
 */
struct DemosViewController {

  /**
   * @brief The superclass.
   * @private
   */
  ViewController viewController;

  /**
   * @brief The interface type.
   * @private
   */
  DemosViewControllerInterface *interface[0];

  /**
   * @brief The demos CollectionView.
   */
  DemosCollectionView *demosList;

  /**
   * @brief Shown in place of `demosList` when there are no demos to display.
   */
  Label *emptyLabel;

  /**
   * @brief The map name filter field.
   */
  TextView *filter;

  /**
   * @brief The Play button, enabled only while a demo is selected.
   */
  Button *play;
};

/**
 * @brief The DemosViewController interface.
 */
struct DemosViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *DemosViewController::_DemosViewController(void)
 * @brief The DemosViewController archetype.
 * @return The DemosViewController Class.
 * @memberof DemosViewController
 */
CGAME_EXPORT Class *_DemosViewController(void);
