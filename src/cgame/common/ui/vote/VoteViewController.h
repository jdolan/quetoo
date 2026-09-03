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

#include <ObjectivelyMVC/ViewController.h>

/**
 * @file
 * @brief The Vote screen: the vote in progress, and calling a new one.
 */

typedef struct VoteViewController VoteViewController;
typedef struct VoteViewControllerInterface VoteViewControllerInterface;

struct VoteViewController {

  /**
   * @brief The superclass.
   */
  ViewController viewController;

  /**
   * @brief The interface type.
   * @protected
   */
  VoteViewControllerInterface *interface[0];

  Label *status;
  Button *yes, *no;

  Select *type;
  Select *map;
  Select *client;
  Slider *value;
  Button *call;
};

struct VoteViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *VoteViewController::_VoteViewController(void)
 * @memberof VoteViewController
 */
CGAME_EXPORT Class *_VoteViewController(void);
