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
#include "bg_nav.h"

#if defined(__CG_LOCAL_H__)

/**
 * @brief Registers the `navInfo` command, which prints the view's position, and the navigation
 * node nearest it with its links, to report a bad or missing node or link.
 */
void Cg_InitNav(void);

/**
 * @brief Builds the navigation graph from the level, as the game does for its bots, and flies its
 * jump pads, the first time that `cg_drawNav` is set on the level, once the frame carries the
 * level's movement parameters, which the flights need. It builds them again if the movement
 * changes, as the game does (`Nav_SameMovement`).
 * @remarks This MUST run on the main thread, because the build can abort with `cgi.Error`.
 */
void Cg_PrepareNav(const ClientFrame *frame);

/**
 * @brief Draws the navigation graph near the view, if `cg_drawNav` is set and it is built.
 * @remarks This MUST run before `Cg_AddSprites`, which submits the sprites that it adds.
 */
void Cg_AddNav(const ClientFrame *frame);

/**
 * @brief Receives one row of `Cg_DescribeNavNode`.
 */
typedef void (*CGameNavRowFunc)(void *data, const char *name, const char *value);

/**
 * @brief Describes the navigation node nearest `position`, one row at a time: its number, position,
 * distance and flags, each of its links, and each push flight that starts within two cells of it.
 * `navInfo` prints the rows, and the diagnostics view shows them while `cg_drawNav` is set.
 */
void Cg_DescribeNavNode(const Vec3 position, CGameNavRowFunc row, void *data);

/**
 * @brief Frees the navigation graph, so that the next level builds its own.
 */
void Cg_FreeNav(void);

#endif
