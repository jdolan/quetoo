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

#include <ObjectivelyMVC/ViewController.h>

/**
 * @file
 * @brief Communication & capture ViewController.
 */

typedef struct CommunicationCaptureViewController CommunicationCaptureViewController;
typedef struct CommunicationCaptureViewControllerInterface CommunicationCaptureViewControllerInterface;

/**
 * @brief The CommunicationCaptureViewController type.
 * @extends ViewController
 */
struct CommunicationCaptureViewController {

  /**
   * @brief The superclass.
   */
  ViewController viewController;

  /**
   * @brief The interface type.
   * @protected
   */
  CommunicationCaptureViewControllerInterface *interface[0];
};

/**
 * @brief The CommunicationCaptureViewController interface.
 */
struct CommunicationCaptureViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *CommunicationCaptureViewController::_CommunicationCaptureViewController(void)
 * @brief The CommunicationCaptureViewController archetype.
 * @return The CommunicationCaptureViewController Class.
 * @memberof CommunicationCaptureViewController
 */
CGAME_EXPORT Class *_CommunicationCaptureViewController(void);
