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

#include "g_types.h"

#if defined(__GAME_LOCAL_H__)

/**
 * @brief The contract every game module implements for the common sources.
 *
 * @details Common code calls these; each module defines them. This is
 * deliberately not a set of #if guards: a guard would put knowledge of every
 * module that will ever exist into shared code, whereas a module implementing a
 * named function keeps that knowledge where it belongs.
 */

/**
 * @brief Disposes of a dropped item that has left the world - fallen into the
 * void, or been caught by a hurt trigger or an explosion.
 * @details A plain deathmatch module frees it. A module with flags returns them
 * to their base instead, and one with techs respawns them.
 */
void G_ResetDroppedItem(g_entity_t *ent);

#endif /* __GAME_LOCAL_H__ */
