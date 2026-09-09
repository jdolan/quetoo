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

#include <ObjectivelyMVC/TableView.h>

/**
 * @file
 * @brief A table of client, renderer and sound counters.
 */

#define DIAGNOSTICS_ROW_NAME 32
#define DIAGNOSTICS_ROW_VALUE 96
#define DIAGNOSTICS_MAX_ROWS 32

typedef struct DiagnosticsView DiagnosticsView;
typedef struct DiagnosticsViewInterface DiagnosticsViewInterface;

/**
 * @brief A two column table of counters: the player's position and speed, the frame, packet
 * and network rates, the view's draw statistics and the stage's channel count.
 * @details Shown only while `cg_draw_diagnostics` is set, and refreshed a few times a second.
 * The packet rate is the client's `packets` count, which this view reads and clears each second.
 * @extends TableView
 */
struct DiagnosticsView {

  /**
   * @brief The superclass.
   */
  TableView tableView;

  /**
   * @brief The interface type.
   * @protected
   */
  DiagnosticsViewInterface *interface[0];

  /**
   * @brief Frames since `time`, the last frame and packet rates, and when they last rolled over.
   */
  int32_t frames, fps, pps;
  uint32_t time;

  /**
   * @brief The count of rows.
   */
  size_t num_rows;

  /**
   * @brief When the rows were last rebuilt.
   */
  uint32_t refresh_time;

  /**
   * @brief The rows, rebuilt on each refresh.
   */
  struct {
    char name[DIAGNOSTICS_ROW_NAME];
    char value[DIAGNOSTICS_ROW_VALUE];
  } rows[DIAGNOSTICS_MAX_ROWS];
};

struct DiagnosticsViewInterface {

  /**
   * @brief The superclass interface.
   */
  TableViewInterface tableViewInterface;
};

CGAME_EXPORT Class *_DiagnosticsView(void);
