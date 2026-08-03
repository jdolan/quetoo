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

extern cvar_t *g_techs;

/**
 * @brief Haste scaling factor.
 */
#define TECH_HASTE_FACTOR 0.75f

/**
 * @brief Resist scaling factors.
 */
#define TECH_RESIST_DAMAGE_FACTOR 0.5f
#define TECH_RESIST_KNOCKBACK_FACTOR 0.75f

/**
 * @brief Strength scaling factors.
 */
#define TECH_STRENGTH_DAMAGE_FACTOR 1.5f
#define TECH_STRENGTH_KNOCKBACK_FACTOR 1.25f

/**
 * @brief Regeneration constants.
 */
#define TECH_REGEN_TICK_TIME 500
#define TECH_REGEN_HEALTH 1

/**
 * @brief Vampire scaling factor.
 */
#define TECH_VAMPIRE_DAMAGE_FACTOR 0.25f

bool G_Tech_Enabled(void);
void G_Tech_Init(void);
void G_Tech_InitMedia(void);
void G_Tech_CheckState(bool enabled_by_default);
void G_Tech_SpawnAll(void);
void G_Tech_ClientThink(g_entity_t *ent);

bool G_HasTech(const g_client_t *cl, g_item_tag_t tech);
const g_item_t *G_GetTech(const g_client_t *cl);
bool G_PickupTech(g_client_t *cl, g_entity_t *ent);
g_entity_t *G_TossTech(g_client_t *cl);
void G_ResetDroppedTech(g_entity_t *ent);
void G_PlayTechSound(g_client_t *cl);

#endif /* __GAME_LOCAL_H__ */
