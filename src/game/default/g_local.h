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

#define __GAME_LOCAL_H__

// this is the game name that we advertise to clients
#ifndef GAME_NAME
	#define GAME_NAME "default"
#endif

#define G_Debug(...) ({ if (gi.DebugMask() & DEBUG_GAME) { gi.Debug_(DEBUG_GAME, __func__, __VA_ARGS__); } })
#define Warn(...) Warn_(__func__, __VA_ARGS__)
#define Error(...) Error_(__func__, __VA_ARGS__)

#include "g_ai.h"
#include "g_ballistics.h"
#include "g_client_chase.h"
#include "g_client_stats.h"
#include "g_client_view.h"
#include "g_client.h"
#include "g_cmd.h"
#include "g_combat.h"
#include "g_entity_func.h"
#include "g_entity_info.h"
#include "g_entity_misc.h"
#include "g_entity_target.h"
#include "g_entity_trigger.h"
#include "g_entity.h"
#include "g_item.h"
#include "g_main.h"
#include "g_map_list.h"
#include "g_mysql.h"
#include "g_physics.h"
#include "g_types.h"
#include "g_util.h"
#include "g_weapon.h"
