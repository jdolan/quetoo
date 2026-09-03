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

#include <ObjectivelyMVC.h>

/**
 * @file
 * @brief Join server ViewController: the known servers in one pane, and the
 * selected one's details in the other.
 */

typedef struct JoinServerViewController JoinServerViewController;
typedef struct JoinServerViewControllerInterface JoinServerViewControllerInterface;

/**
 * @brief The JoinServerViewController type.
 * @extends ViewController
 * @ingroup
 */
struct JoinServerViewController {

  /**
   * @brief The superclass.
   * @private
   */
  ViewController viewController;

  /**
   * @brief The interface type.
   * @private
   */
  JoinServerViewControllerInterface *interface[0];

  /**
   * @brief A copy of the client's servers list, for sorting, filtering, etc.
   */
  PointerArray *servers;

  /**
   * @brief The servers TableView.
   */
  TableView *serversTableView;

  /**
   * @brief The list's empty state, shown in place of the table rather than
   * beside it, since one is meaningful only when the other is not.
   */
  Label *emptyLabel;

  /**
   * @brief The details pane, populated from the current selection.
   */
  Label *hostnameLabel, *addressLabel, *hintLabel, *sourceLabel;
  Label *mapLabel, *gameplayLabel, *movementLabel, *playersLabel, *pingLabel;

  /**
   * @brief The selected server's mapshot, when its map is installed locally.
   */
  ImageView *mapshotView;

  /**
   * @brief The part of the details pane that only means something once a
   * server is selected, hidden together until then.
   */
  View *detailGrid;

  /**
   * @brief The details pane's action.
   */
  Button *connectButton;

  /**
   * @brief The max ping slider, which doubles as the ping colour threshold.
   * @remarks Typed as its Slider superclass: the JSON declares a CvarSlider,
   * which writes `cg_quick_join_max_ping` itself, and `CvarSlider.h` is not
   * part of the umbrella ObjectivelyMVC header this one includes.
   */
  Slider *maxPingSlider;

  /**
   * @brief The selected server's hostname.
   * @details The selection is held by name rather than by row index, so that
   * it survives a re-sort and a refresh. Empty when nothing is selected.
   */
  char selectedHostname[48];
};

/**
 * @brief The JoinServerViewController interface.
 */
struct JoinServerViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

  /**
   * @fn void JoinServerViewController::reloadServers(JoinServerViewController *self)
   * @brief Reloads the list of known servers.
   * @param self The JoinServerViewController.
   * @memberof JoinServerViewController
   */
  void (*reloadServers)(JoinServerViewController *self);
};

/**
 * @fn Class *JoinServerViewController::_JoinServerViewController(void)
 * @brief The JoinServerViewController archetype.
 * @return The JoinServerViewController Class.
 * @memberof JoinServerViewController
 */
CGAME_EXPORT Class *_JoinServerViewController(void);

