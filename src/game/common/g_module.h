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
} g_client_server_fields_t;

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
} g_entity_server_fields_t;

#define G_ASSERT_SERVER_FIELD(prefix, field)                                   \
  _Static_assert(offsetof(prefix##_t, field) ==                                \
                     offsetof(prefix##_server_fields_t, field),                \
                 #prefix "::" #field " must keep the offset the server "       \
                 "expects; the server's fields come first, in game.h order")

G_ASSERT_SERVER_FIELD(g_client, entity);
G_ASSERT_SERVER_FIELD(g_client, ps);
G_ASSERT_SERVER_FIELD(g_client, ping);
G_ASSERT_SERVER_FIELD(g_client, score);
G_ASSERT_SERVER_FIELD(g_client, user_info);
G_ASSERT_SERVER_FIELD(g_client, in_use);
G_ASSERT_SERVER_FIELD(g_client, ai);

G_ASSERT_SERVER_FIELD(g_entity, def);
G_ASSERT_SERVER_FIELD(g_entity, classname);
G_ASSERT_SERVER_FIELD(g_entity, model);
G_ASSERT_SERVER_FIELD(g_entity, s);
G_ASSERT_SERVER_FIELD(g_entity, in_use);
G_ASSERT_SERVER_FIELD(g_entity, sv_flags);
G_ASSERT_SERVER_FIELD(g_entity, bounds);
G_ASSERT_SERVER_FIELD(g_entity, abs_bounds);
G_ASSERT_SERVER_FIELD(g_entity, size);
G_ASSERT_SERVER_FIELD(g_entity, solid);
G_ASSERT_SERVER_FIELD(g_entity, owner);
G_ASSERT_SERVER_FIELD(g_entity, client);

#endif /* __GAME_LOCAL_H__ */
