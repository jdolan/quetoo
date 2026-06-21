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

#include "cg_types.h"

#include <ObjectivelyMVC/TableView.h>
#include <ObjectivelyMVC/ViewController.h>

/**
 * @file
 * @brief Demo browser ViewController: lists recorded demos and plays them.
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
   */
  ViewController viewController;

  /**
   * @brief The interface.
   * @protected
   */
  DemosViewControllerInterface *interface;

  /**
   * @brief The demos table.
   */
  TableView *demos;

  /**
   * @brief The recorded demo names (String objects), one per row.
   */
  Array *names;

  /**
   * @brief The selected row, or -1 if none.
   */
  int32_t selectedRow;
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
