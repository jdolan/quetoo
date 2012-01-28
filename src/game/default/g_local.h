/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __G_LOCAL_H__
#define __G_LOCAL_H__

#include "shared.h"
#include "game/game.h"

#ifdef HAVE_MYSQL
#include <mysql.h>
extern MYSQL *mysql;
extern char sql[512];
#endif

#include "g_types.h"

// this is the game name that we advertise to clients
#define GAME_NAME "default"

#include "g_ballistics.h"
#include "g_chase.h"
#include "g_client.h"
#include "g_combat.h"
#include "g_commands.h"
#include "g_entity.h"
#include "g_entity_func.h"
#include "g_entity_info.h"
#include "g_entity_misc.h"
#include "g_entity_target.h"
#include "g_entity_trigger.h"
#include "g_items.h"
#include "g_main.h"
#include "g_physics.h"
#include "g_stats.h"
#include "g_utils.h"
#include "g_view.h"
#include "g_weapons.h"

#endif /* __G_LOCAL_H__ */
