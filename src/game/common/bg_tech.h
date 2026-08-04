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

#include "shared/shared.h"

/**
 * @brief Tech powerups are an optional feature. A game module opts in by adding
 * g_tech.c to its _SOURCES and -DG_TECH to its AM_CPPFLAGS, then embedding
 * `g_client_tech_t` in its `g_client_t`. The MSVS build sets QuetooGameTech and
 * G_TECH; the Xcode build adds the source to the target and G_TECH to its
 * defines.
 *
 * @details This header is deliberately free of game types so that g_types.h may
 * include it before defining them. The techs own everything else they need:
 * their cvar, their sound indices and their enabled state all live in g_tech.c.
 * The module supplies only this per-client state, plus the ITEM_TYPE_TECH item
 * type, the TECH_FIRST..TECH_LAST item tags, the item definitions themselves and
 * the STAT_TECH value, which is read off the wire by the client game and so must
 * be numbered by the module.
 */

/**
 * @brief Per-client tech state. This is reset when the client respawns.
 */
typedef struct {
  /**
   * @brief Next regeneration tick time, for the regeneration tech.
   */
  uint32_t regen_time;

  /**
   * @brief Next time a tech powerup sound may play.
   */
  uint32_t sound_time;
} g_client_tech_t;
