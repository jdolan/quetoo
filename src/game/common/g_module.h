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
 * @brief The fields the server reads out of a client and an entity, mirroring
 * its own declarations in game.h.
 * @details The server declares g_client_t and g_entity_t itself, holding only
 * these fields, and reads them at the offsets that declaration produces. A
 * module extends the structures by *appending*, so its definitions MUST begin
 * with these fields, in this order.
 *
 * Getting it wrong compiles cleanly and then presents as a network fault rather
 * than a memory one. When the ctf module declared its grapple state ahead of
 * `entity`, the server read `in_use` from the wrong offset, took a freshly
 * connected client for a disconnected bot, and recycled the slot; clients
 * completed the handshake and then timed out against a server that had already
 * forgotten them. The assertions below make that a build failure instead.
 */
typedef struct {
  g_entity_t *entity;
  player_state_t ps;
  uint32_t ping;
  int16_t score;
  char user_info[MAX_INFO_STRING_STRING];
  bool in_use;
  void *ai;
} g_client_t_fields_t;

typedef struct {
  const cm_entity_t *def;
  const char *classname;
  const char *model;
  entity_state_t s;
  bool in_use;
  uint32_t sv_flags;
  box3_t bounds;
  box3_t abs_bounds;
  vec3_t size;
  solid_t solid;
  g_entity_t *owner;
  g_client_t *client;
} g_entity_t_fields_t;

#define G_ASSERT_SERVER_FIELD(type, field) \
  static_assert(offsetof(type, field) == offsetof(type##_fields_t, field) \
    && sizeof(((type *) 0)->field) == sizeof(((type##_fields_t *) 0)->field), \
      #type "::" #field " must keep the offset and the size the " \
      "server expects; the server's fields come first, in game.h order")

G_ASSERT_SERVER_FIELD(g_client_t, entity);
G_ASSERT_SERVER_FIELD(g_client_t, ps);
G_ASSERT_SERVER_FIELD(g_client_t, ping);
G_ASSERT_SERVER_FIELD(g_client_t, score);
G_ASSERT_SERVER_FIELD(g_client_t, user_info);
G_ASSERT_SERVER_FIELD(g_client_t, in_use);
G_ASSERT_SERVER_FIELD(g_client_t, ai);

G_ASSERT_SERVER_FIELD(g_entity_t, def);
G_ASSERT_SERVER_FIELD(g_entity_t, classname);
G_ASSERT_SERVER_FIELD(g_entity_t, model);
G_ASSERT_SERVER_FIELD(g_entity_t, s);
G_ASSERT_SERVER_FIELD(g_entity_t, in_use);
G_ASSERT_SERVER_FIELD(g_entity_t, sv_flags);
G_ASSERT_SERVER_FIELD(g_entity_t, bounds);
G_ASSERT_SERVER_FIELD(g_entity_t, abs_bounds);
G_ASSERT_SERVER_FIELD(g_entity_t, size);
G_ASSERT_SERVER_FIELD(g_entity_t, solid);
G_ASSERT_SERVER_FIELD(g_entity_t, owner);
G_ASSERT_SERVER_FIELD(g_entity_t, client);

/**
 * @brief The contract every game module implements for the common sources.
 *
 * @details Common code calls these; each module defines them. This is
 * deliberately not a set of #if guards: a guard would put knowledge of every
 * module that will ever exist into shared code, whereas a module implementing a
 * named function keeps that knowledge where it belongs.
 *
 * Variation points that several optional features may each want a say in are
 * chainable hooks rather than named functions. Common holds the default
 * behaviour; a feature installs itself over the top in its own `_Init`, keeping
 * the previous value to call as super. Composition then falls out: a module with
 * flags and techs gets both, one with only techs gets only that, and neither
 * feature has to know the other exists.
 *
 * Hooks MUST be installed from `G_Init`, once per module load. Installing from
 * anything per-level would grow the chain on every map restart. Chain order is
 * installation order, so the order of the `_Init` calls in a module's `G_Init`
 * is part of its behaviour.
 */

/**
 * @brief Disposes of a dropped item that has left the world - fallen into the
 * void, or been caught by a hurt trigger or an explosion.
 * @details A plain deathmatch module frees it. A module with flags returns them
 * to their base instead, and one with techs respawns them.
 */
typedef void (*ResetDroppedItem)(g_entity_t *ent);

extern ResetDroppedItem G_ResetDroppedItem;

/**
 * @brief Drops the named item from the client's inventory, reporting to them
 * when they can not.
 * @details Deathmatch resolves the name against the item list. Features that
 * answer to a category rather than an item name, such as the "flag" a client
 * happens to be carrying, resolve it to a real item name and defer.
 */
typedef void (*DropInventoryItem)(g_client_t *cl, const char *name);

extern DropInventoryItem G_DropInventoryItem;

/**
 * @brief Scales the damage and the knockback an attack is about to impart,
 * before friendly fire and self damage are resolved.
 * @details The quad damage powerup is the default. A feature carrying its own
 * modifiers, such as the resist and strength techs, installs over the top;
 * because the modifiers multiply, where it calls super decides only how the
 * integer truncation falls.
 */
typedef void (*ModifyDamage)(g_entity_t *target, g_entity_t *attacker, int32_t *damage, int32_t *knockback);

extern ModifyDamage G_ModifyDamage;

/**
 * @brief Applies whichever of a feature's own cvars have been modified, once
 * per server frame.
 * @return True if the change requires the level to restart.
 * @details A feature announces the change itself, so that the module enforcing
 * the rules does not have to know which cvars exist. Every implementation MUST
 * clear the `modified` flag of each cvar it consumes, or it will announce the
 * same change on every frame.
 */
typedef bool (*CheckCvars)(void);

extern CheckCvars G_CheckCvars;

/**
 * @brief Decides whether the level has been won, once per server frame.
 * @details A single owner: frag limit and capture limit are answers to the same
 * question, not additions to each other, so a module that plays for captures
 * replaces this rather than chaining onto it. Announce the reason before
 * returning true; the caller only ends the level.
 */
typedef bool (*CheckWinCondition)(void);

extern CheckWinCondition G_CheckWinCondition;

#endif
