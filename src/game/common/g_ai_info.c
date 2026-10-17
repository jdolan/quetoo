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

#include <Objectively/List.h>

#include "g_local.h"

static Cvar *g_aiNamePrefix;

/**
 * @brief The roster file, in the map list format: one entity of keys for each AI player.
 */
#define AI_ROSTER_FILE "ai.lst"

/**
 * @brief The AI players of `ai.lst`, which live as long as the game module, because each AI
 * client points to its own.
 */
static GameAiRoster *aiRoster;

/**
 * @brief The number of AI players in `aiRoster`.
 */
static uint32_t aiRosterCount;

/**
 * @brief Shuffled order in which roster entries are handed out. Reshuffled
 * each time it is exhausted so that every entry is used exactly once per
 * cycle, in a random order, before any entry repeats.
 */
static uint32_t *aiRosterOrder;

/**
 * @brief Index of the next entry to hand out from aiRosterOrder.
 */
static uint32_t aiRosterIndex;

/**
 * @brief Reshuffles aiRosterOrder in place using a Fisher-Yates shuffle.
 */
static void G_Ai_ShuffleRoster(void) {

  for (uint32_t i = 0; i < aiRosterCount; i++) {
    aiRosterOrder[i] = i;
  }

  for (uint32_t i = aiRosterCount - 1; i > 0 && i < aiRosterCount; i--) {
    const uint32_t j = RandomRangeu(0, i + 1);
    const uint32_t tmp = aiRosterOrder[i];
    aiRosterOrder[i] = aiRosterOrder[j];
    aiRosterOrder[j] = tmp;
  }

  aiRosterIndex = 0;
}

/**
 * @return True if the string is a tint: six hex digits.
 */
static bool G_Ai_IsTint(const char *s) {
  return strlen(s) == 6 && strspn(s, "0123456789abcdefABCDEF") == 6;
}

/**
 * @brief Copies the entry's optional tint or hue key into `out`, if it is valid, and warns of one
 * that is not, which is then left empty, so that the AI player gets a random one.
 */
static void G_Ai_LoadRoster_color(const Entity *e, const char *name, const char *key, char *out,
                                  size_t size) {

  const char *value = gi.EntityValue(e, key)->string;
  if (!*value) {
    return;
  }

  if (!Str_Compare(key, "color")) {
    char *end;
    const long hue = strtol(value, &end, 10);
    if (end == value || *end || hue < 0 || hue > 360) {
      G_Warn("%s: %s has %s \"%s\", which is not a hue from 0 to 360; it is random\n",
             AI_ROSTER_FILE, name, key, value);
      return;
    }
    Str_Format(out, size, "%ld", hue);
    return;
  }

  if (!G_Ai_IsTint(value)) {
    G_Warn("%s: %s has %s \"%s\", which is not six hex digits; it is random\n", AI_ROSTER_FILE,
           name, key, value);
    return;
  }

  Str_Copy(out, value, size);
}

/**
 * @brief Loads the roster from `ai.lst`. An entry without a name or skin, or with a trait that is
 * not a number from 0 to 1, is skipped with a warning.
 */
static void G_Ai_LoadRoster(void) {

  aiRoster = NULL;
  aiRosterOrder = NULL;
  aiRosterCount = 0;

  void *buffer;
  if (gi.LoadFile(AI_ROSTER_FILE, &buffer) == -1) {
    G_Warn("Couldn't load %s: no AI players can join\n", AI_ROSTER_FILE);
    return;
  }

  List *entities = gi.LoadEntities(buffer);
  gi.FreeFile(buffer);

  aiRoster = gi.Malloc(sizeof(GameAiRoster) * Maxi(1, (int32_t) entities->count), MEM_TAG_AI);
  aiRosterOrder = gi.Malloc(sizeof(uint32_t) * Maxi(1, (int32_t) entities->count), MEM_TAG_AI);

  int32_t index = 0;
  for (const ListNode *node = entities->head; node; node = node->next, index++) {
    const Entity *e = node->element;

    const char *name = gi.EntityValue(e, "name")->string;
    const char *skin = gi.EntityValue(e, "skin")->string;

    if (!*name || !*skin) {
      G_Warn("%s: entry %d has no name or no skin, and is skipped\n", AI_ROSTER_FILE, index);
      continue;
    }

    GameAiRoster *entry = &aiRoster[aiRosterCount];
    memset(entry, 0, sizeof(*entry));

    Str_Copy(entry->name, name, sizeof(entry->name));
    Str_Copy(entry->skin, skin, sizeof(entry->skin));
    Str_Copy(entry->guid, gi.EntityValue(e, "guid")->string, sizeof(entry->guid));

    const char *traits[] = { "skill", "aggression", "awareness" };
    float *values[] = { &entry->skill, &entry->aggression, &entry->awareness };

    bool valid = true;
    for (size_t i = 0; i < lengthof(traits); i++) {
      const char *string = gi.EntityValue(e, traits[i])->string;
      char *end;
      const float value = strtof(string, &end);
      if (end == string || *end || !(value >= 0.f && value <= 1.f)) {
        G_Warn("%s: %s has %s \"%s\", which is not a number from 0 to 1, and is skipped\n",
               AI_ROSTER_FILE, name, traits[i], string);
        valid = false;
        break;
      }
      *values[i] = value;
    }

    if (!valid) {
      continue;
    }

    G_Ai_LoadRoster_color(e, name, "color", entry->color, sizeof(entry->color));
    G_Ai_LoadRoster_color(e, name, "helmet", entry->helmet, sizeof(entry->helmet));
    G_Ai_LoadRoster_color(e, name, "shirt", entry->shirt, sizeof(entry->shirt));
    G_Ai_LoadRoster_color(e, name, "pants", entry->pants, sizeof(entry->pants));

    aiRosterCount++;
  }

  for (const ListNode *node = entities->head; node; node = node->next) {
    gi.FreeEntity(node->element);
  }

  release(entities);

  if (aiRosterCount == 0) {
    G_Warn("%s has no valid AI players: no AI players can join\n", AI_ROSTER_FILE);
  }
}

/**
 * @return The number of AI players in the roster.
 */
uint32_t G_Ai_RosterCount(void) {
  return aiRosterCount;
}

/**
 * @brief Returns true if the given name is already in use by a connected client other than @p cl.
 */
static _Bool G_Ai_NameInUse(const GameClient *cl, const char *name) {

  G_ForEachClient(other, {
    if (other == cl) {
      continue;
    }
    char otherName[MAX_INFO_STRING_VALUE];
    InfoString_Get(other->userInfo, "name", otherName, sizeof(otherName));
    if (Str_Compare(otherName, name) == 0) {
      return true;
    }
  });

  return false;
}

/**
 * @return A random tint: six hex digits.
 */
static const char *G_Ai_RandomTint(void) {
  return va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256));
}

/**
 * @brief Create the user info for the specified bot from the given roster entry.
 *
 * @details The entry's hue and tints apply where it sets them, and random ones elsewhere. If the
 * base name is already taken by another connected client, appends " 1", " 2", etc. until a unique
 * name is found.
 */
void G_Ai_GetRosterInfo(const GameClient *cl, const GameAiRoster *entry, char *info) {

  Str_Copy(info, DEFAULT_AI_INFO, MAX_INFO_STRING_STRING);

  InfoString_Set(info, "skin", entry->skin);
  InfoString_Set(info, "guid", entry->guid);
  InfoString_Set(info, "color", *entry->color ? entry->color : va("%u", RandomRangeu(0, 360)));
  InfoString_Set(info, "hand", va("%u", RandomRangeu(0, 3)));
  InfoString_Set(info, "helmet", *entry->helmet ? entry->helmet : G_Ai_RandomTint());
  InfoString_Set(info, "shirt", *entry->shirt ? entry->shirt : G_Ai_RandomTint());
  InfoString_Set(info, "pants", *entry->pants ? entry->pants : G_Ai_RandomTint());

  char name[MAX_INFO_STRING_VALUE];
  Str_Format(name, sizeof(name), "%s%s", g_aiNamePrefix->string, entry->name);
  for (uint32_t suffix = 1; G_Ai_NameInUse(cl, name); suffix++) {
    Str_Format(name, sizeof(name), "%s%s %u", g_aiNamePrefix->string, entry->name, suffix);
  }

  InfoString_Set(info, "name", name);
}

/**
 * @return True if a connected AI player has the roster entry.
 */
static bool G_Ai_RosterInGame(const GameAiRoster *entry) {

  G_ForEachClient(cl, {
    if (cl->ai && cl->ai->roster == entry) {
      return true;
    }
  });

  return false;
}

/**
 * @return The level's first AI player, by the `ai` key, that is not in the game, or NULL.
 */
static const GameAiRoster *G_Ai_GetRoster_level(void) {

  char names[MAX_STRING_CHARS];
  Str_Copy(names, gameLevel.ai, sizeof(names));

  char *save;
  for (char *name = Str_Tokenize(names, " \t", &save); name; name = Str_Tokenize(NULL, " \t", &save)) {
    const GameAiRoster *entry = G_Ai_FindRoster(name);
    if (entry && !G_Ai_RosterInGame(entry)) {
      return entry;
    }
  }

  return NULL;
}

/**
 * @brief Create the user info for the specified bot and return its roster entry.
 *
 * @details Assigns the level's first AI player, by the `ai` key, that is not in the game. Once
 * none is left, assigns the next roster entry from a shuffled order that is not in the game, so
 * that entries are handed out in a random sequence without repeating until every entry has been
 * used, at which point the order is reshuffled. If every entry is in the game, one repeats.
 * @return The roster entry, or NULL if the roster is empty.
 */
const GameAiRoster *G_Ai_GetRoster(const GameClient *cl, char *info) {

  if (aiRosterCount == 0) {
    return NULL;
  }

  const GameAiRoster *entry = G_Ai_GetRoster_level();

  bool reshuffled = false;

  for (uint32_t i = 0; !entry && i < aiRosterCount; i++) {
    if (aiRosterIndex == aiRosterCount) {
      G_Ai_ShuffleRoster();
      if (!reshuffled) {
        reshuffled = true;
        i = 0;
      }
    }

    const GameAiRoster *next = &aiRoster[aiRosterOrder[aiRosterIndex++]];

    if (!G_Ai_RosterInGame(next) || i == aiRosterCount - 1) {
      entry = next;
    }
  }

  G_Ai_GetRosterInfo(cl, entry, info);

  return entry;
}

/**
 * @return The roster entry with the given name, ignoring case, or NULL.
 */
const GameAiRoster *G_Ai_FindRoster(const char *name) {

  for (uint32_t i = 0; i < aiRosterCount; i++) {
    if (Str_CaseCompare(aiRoster[i].name, name) == 0) {
      return &aiRoster[i];
    }
  }

  return NULL;
}

/**
 * @return The number of distinct roster entries that the level's `ai` key names.
 */
int32_t G_Ai_LevelRosterCount(void) {

  const GameAiRoster *named[MAX_CLIENTS];
  int32_t count = 0;

  char names[MAX_STRING_CHARS];
  Str_Copy(names, gameLevel.ai, sizeof(names));

  char *save;
  for (char *name = Str_Tokenize(names, " \t", &save); name && count < MAX_CLIENTS;
       name = Str_Tokenize(NULL, " \t", &save)) {
    const GameAiRoster *entry = G_Ai_FindRoster(name);
    if (!entry) {
      continue;
    }

    int32_t i = 0;
    while (i < count && named[i] != entry) {
      i++;
    }

    if (i == count) {
      named[count++] = entry;
    }
  }

  return count;
}

/**
 * @return True if the level's `ai` key names the roster entry.
 */
bool G_Ai_IsLevelRoster(const GameAiRoster *entry) {

  char names[MAX_STRING_CHARS];
  Str_Copy(names, gameLevel.ai, sizeof(names));

  char *save;
  for (char *name = Str_Tokenize(names, " \t", &save); name; name = Str_Tokenize(NULL, " \t", &save)) {
    if (G_Ai_FindRoster(name) == entry) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Warns of each name in the level's `ai` key that the roster does not have.
 */
void G_Ai_CheckLevelRoster(void) {

  char names[MAX_STRING_CHARS];
  Str_Copy(names, gameLevel.ai, sizeof(names));

  char *save;
  for (char *name = Str_Tokenize(names, " \t", &save); name; name = Str_Tokenize(NULL, " \t", &save)) {
    if (!G_Ai_FindRoster(name)) {
      G_Warn("%s names AI player \"%s\", which %s does not have\n", gameLevel.name, name,
             AI_ROSTER_FILE);
    }
  }
}

/**
 * @brief Prints the name and traits of each roster entry.
 */
void G_Ai_PrintRoster(void) {

  if (aiRosterCount == 0) {
    gi.Print("No AI roster is loaded: see %s\n", AI_ROSTER_FILE);
    return;
  }

  gi.Print("  name         skill  aggression  awareness\n");

  for (uint32_t i = 0; i < aiRosterCount; i++) {
    const GameAiRoster *entry = &aiRoster[i];
    gi.Print("  %-12s %.2f   %.2f        %.2f\n", entry->name, entry->skill, entry->aggression, entry->awareness);
  }
}

/**
 * @brief Initializes the AI name prefix console variable, and loads the roster.
 */
void G_Ai_InitSkins(void) {
  g_aiNamePrefix = gi.AddCvar("g_aiNamePrefix", "^0[^1BOT^0] ^7", 0, NULL);
  G_Ai_LoadRoster();
  G_Ai_ShuffleRoster();
}

/**
 * @brief Shuts down AI skin and name resources. `G_Ai_Shutdown` frees the roster with the rest of
 * `MEM_TAG_AI`.
 */
void G_Ai_ShutdownSkins(void) {
  aiRoster = NULL;
  aiRosterOrder = NULL;
  aiRosterCount = 0;
}
