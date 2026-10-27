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

#include "collision/cm_types.h"
#include "g_types.h"

/**
 * @brief A game's `game.lst`: one entity of keys, in the map list format, that describes the game
 * for the menu and declares what it supports. Every key is optional.
 */
typedef struct {
  /**
   * @brief The title, or the game's directory name if unset.
   */
  char name[MAX_QPATH];

  /**
   * @brief The text on the game's card, with each `\n` made a line break.
   */
  char description[MAX_STRING_CHARS];

  /**
   * @brief A PNG or JPG, relative to the game's directory, or empty.
   */
  char image[MAX_QPATH];

  /**
   * @brief The gameplay modes, as canonical `g_gameplay` names separated by spaces, or empty for
   * all of them.
   */
  char gameplay[MAX_STRING_CHARS];

  /**
   * @brief The tokens of a map's `games` key that Create Server lists, separated by spaces.
   */
  char maps[MAX_STRING_CHARS];

  /**
   * @brief False if the game offers no Practice.
   */
  bool practice;
} GameInfo;

/**
 * @brief The `EntityValue` import of the game or the client game.
 */
typedef const Entity *(*GameInfoEntityValueFunc)(const Entity *entity, const char *key);

/**
 * @brief Fills `info` from the first entity of a game's `game.lst`.
 * @param info The info.
 * @param game The game's directory name, the fallback for `name` and `maps`.
 * @param entity The entity, or `NULL` if the game has no `game.lst`.
 * @param value The `EntityValue` import.
 */
static inline void GameInfo_Load(GameInfo *info, const char *game, const Entity *entity, GameInfoEntityValueFunc value) {

  *info = (GameInfo) {
    .practice = true
  };

  Str_Copy(info->name, game, sizeof(info->name));
  Str_Copy(info->maps, game, sizeof(info->maps));

  if (entity == NULL) {
    return;
  }

  const Entity *pair;

  if ((pair = value(entity, "name"))->nullableString) {
    Str_Copy(info->name, pair->string, sizeof(info->name));
  }

  if ((pair = value(entity, "description"))->nullableString) {
    char *out = info->description;
    for (const char *in = pair->string; *in && out < info->description + sizeof(info->description) - 1; in++) {
      if (in[0] == '\\' && in[1] == 'n') {
        *out++ = '\n';
        in++;
      } else {
        *out++ = *in;
      }
    }
    *out = '\0';
  }

  if ((pair = value(entity, "image"))->nullableString) {
    Str_Copy(info->image, pair->string, sizeof(info->image));
  }

  if ((pair = value(entity, "gameplay"))->nullableString) {
    Str_Copy(info->gameplay, pair->string, sizeof(info->gameplay));
  }

  if ((pair = value(entity, "maps"))->nullableString) {
    Str_Copy(info->maps, pair->string, sizeof(info->maps));
  }

  if ((pair = value(entity, "practice"))->nullableString) {
    info->practice = pair->integer != 0;
  }
}

/**
 * @return The first name in `gameplay` that is not a canonical `g_gameplay` name, or `NULL`.
 * @param info The info.
 * @param token A buffer for the name.
 * @param size The size of `token`.
 */
static inline const char *GameInfo_UnknownGamePlay(const GameInfo *info, char *token, size_t size) {

  char names[MAX_STRING_CHARS];
  Str_Copy(names, info->gameplay, sizeof(names));

  char *savePtr;
  for (const char *name = Str_Tokenize(names, " ", &savePtr); name; name = Str_Tokenize(NULL, " ", &savePtr)) {

    bool known = false;
    for (size_t i = 0; i < lengthof(gameplays) && !known; i++) {
      known = !Str_Compare(name, gameplays[i].name);
    }

    if (!known) {
      Str_Copy(token, name, size);
      return token;
    }
  }

  return NULL;
}

/**
 * @return True if the game supports the gameplay mode `id`.
 */
static inline bool GameInfo_HasGamePlay(const GameInfo *info, GamePlayId id) {

  if (*info->gameplay == '\0') {
    return true;
  }

  for (size_t i = 0; i < lengthof(gameplays); i++) {
    if (gameplays[i].id == id) {
      return Str_HasToken(info->gameplay, gameplays[i].name);
    }
  }

  return false;
}

/**
 * @return The mode the game runs for a request of `id`: `id` if the game supports it, else `id`
 * with team play switched, else the first mode the game lists.
 */
static inline GamePlayId GameInfo_ClampGamePlay(const GameInfo *info, GamePlayId id) {

  if (GameInfo_HasGamePlay(info, id)) {
    return id;
  }

  if (GameInfo_HasGamePlay(info, id ^ GAMEPLAY_TEAMS)) {
    return id ^ GAMEPLAY_TEAMS;
  }

  for (size_t i = 0; i < lengthof(gameplays); i++) {
    if (GameInfo_HasGamePlay(info, gameplays[i].id)) {
      return gameplays[i].id;
    }
  }

  return id;
}

/**
 * @return True if a map whose `games` key is `games` is made for this game.
 */
static inline bool GameInfo_HasMap(const GameInfo *info, const char *games) {

  char maps[MAX_STRING_CHARS];
  Str_Copy(maps, info->maps, sizeof(maps));

  char *savePtr;
  for (const char *token = Str_Tokenize(maps, " ", &savePtr); token; token = Str_Tokenize(NULL, " ", &savePtr)) {
    if (Str_HasToken(games, token)) {
      return true;
    }
  }

  return false;
}
