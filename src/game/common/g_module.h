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

#include "bg_pmove.h"
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

/**
 * @brief Names the gameplay the server is running, for its info strings.
 * @param name The buffer to format into, holding the gameplay name.
 * @param size The size of that buffer.
 * @details Chainable so that a feature can qualify what it was handed, but a
 * feature that renames the gameplay outright does not defer to super.
 */
typedef void (*FormatGameName)(char *name, size_t size);

extern FormatGameName G_FormatGameName;

/**
 * @brief Tosses whatever the client is carrying that must not leave play with
 * them, when they change team or become a spectator.
 * @details The quad damage powerup is the default. Flags, techs and the grapple
 * each add their own, which is why this is a chain rather than a list of calls
 * in whichever command happens to need it.
 */
typedef void (*TossInventory)(g_client_t *cl);

extern TossInventory G_TossInventory;

/**
 * @brief Places an item in the world for the start of a level, deciding whether
 * it can be seen and touched.
 * @details The whole of the deathmatch placement is the default, and a feature
 * that has more to say about its own items installs over the top. Anything it
 * changes after deferring to super MUST be linked again, because super links.
 */
typedef void (*ResetItem)(g_entity_t *ent);

extern ResetItem G_ResetItem;

/**
 * @brief Decides whether the current gameplay withholds an item from the level
 * altogether.
 * @details Arena and instagib withhold everything by default. A feature whose
 * items are the point of the level, such as the flags, exempts its own.
 */
typedef bool (*InhibitItem)(const g_entity_t *ent);

extern InhibitItem G_InhibitItem;

/**
 * @brief Fills in an item's behaviour - how it is picked up, and how it is
 * dropped - from its type.
 * @details The default answers for the deathmatch item types and errors on any
 * type it does not recognise, so a feature bringing its own item type MUST
 * install ahead of it and answer for that type rather than deferring.
 */
typedef void (*InitItem)(g_item_t *it);

extern InitItem G_InitItem;

/**
 * @brief Indexes the models and sounds the module needs for the level ahead.
 * @details The whole of the deathmatch media is the default; a feature indexes
 * its own on top, and keeps the indices itself rather than growing `g_media`.
 */
typedef void (*InitMedia)(void);

extern InitMedia G_InitMedia;

/**
 * @brief Resolves the settings a feature holds for the level ahead, after the
 * worldspawn has been read, and again whenever the game restarts.
 * @details Nothing to do by default. A feature decides here whether it is
 * enabled this level, and publishes any config strings the client needs.
 */
typedef void (*ConfigureLevel)(void);

extern ConfigureLevel G_ConfigureLevel;

/**
 * @brief Seeds the player move with the state it should start from, before the
 * client's command is applied.
 * @details The default hands the move the entity's own velocity. A feature that
 * takes the movement over, as the grapple does while pulling, sets the move type
 * and the velocity it wants and does not defer to super.
 */
typedef void (*PrepareMove)(g_client_t *cl, pm_move_t *pm);

extern PrepareMove G_PrepareMove;

/**
 * @brief The module's own initialization, called once per module load from
 * `G_Init`, after the features common ships have installed themselves.
 * @details This is where a module installs the hooks for behaviour that is its
 * own rather than one of common's features. It exists because a mod Quetoo does
 * not ship can only add files to its own directory: it cannot add a call to
 * `G_Init`, and a guard named after it could never be committed here, so without
 * this seam its only way to install a hook would be to fork `g_main.c`.
 *
 * Every module MUST define it, even if the body is empty. A missing definition
 * is a link error, which is the right way to tell a new module what it owes.
 * Installing from here rather than from `G_Init` also means a module's hooks sit
 * at the head of every chain, so they may wrap a shipped feature.
 */
void G_Module_Init(void);

/**
 * @brief The module's own shutdown, called from `G_Shutdown` before the game's
 * memory tags are freed, so that a module may still touch what it allocated.
 * @details Every module MUST define it, even if the body is empty.
 *
 * This is for what a module allocated or opened, and **MUST NOT uninstall
 * hooks**. `G_Init` and `G_Shutdown` run on every server initialization, while a
 * hook installs exactly once per module image, behind a `static bool installed` -
 * so uninstalling here would tear a link out of a chain that the next `G_Init`
 * declines to rebuild. Chains are not unwound; they are built once and left.
 */
void G_Module_Shutdown(void);

#endif
