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

#include "cg_local.h"

Cvar *cg_practice;
Cvar *cg_practiceLadder;

/**
 * @brief An entry the player has cleared, under the gameplay they cleared it in.
 */
typedef struct {
  char gameplay[MAX_QPATH];
  char name[MAX_QPATH];
  char ai[MAX_STRING_CHARS];
} CGamePracticeClear;

static struct {
  char ladderPath[MAX_QPATH];
  char progressPath[MAX_QPATH];

  CGamePracticeEntry *ladder;
  size_t ladderLength;
  int32_t maxClients;

  CGamePracticeClear *clears;
  size_t numClears;
} module;

/**
 * @brief The integer `key` holds in `entity`, or -1 if it holds none.
 */
static int32_t Cg_Practice_Integer(const Entity *entity, const char *key) {

  const Entity *value = cgi.EntityValue(entity, key);
  return (value->parsed & ENTITY_INTEGER) ? value->integer : -1;
}

/**
 * @brief Loads the entities of `path`, or returns `NULL` if it does not exist or is empty.
 */
static List *Cg_Practice_LoadEntities(const char *path) {

  void *buffer = NULL;
  if (cgi.LoadFile(path, &buffer) <= 0) {
    if (buffer) {
      cgi.FreeFile(buffer);
    }
    return NULL;
  }

  List *entities = cgi.LoadEntities(buffer);
  cgi.FreeFile(buffer);

  return entities;
}

/**
 * @brief Frees the entities that `Cg_Practice_LoadEntities` loaded.
 */
static void Cg_Practice_FreeEntities(List *entities) {

  for (ListNode *node = entities->head; node; node = node->next) {
    cgi.FreeEntity(node->element);
  }

  release(entities);
}

/**
 * @brief Loads the ladder. An entry without a map or AI players, or with no tier, is skipped.
 */
static void Cg_Practice_LoadLadder(void) {

  free(module.ladder);
  module.ladder = NULL;
  module.ladderLength = 0;
  module.maxClients = 0;

  List *entities = Cg_Practice_LoadEntities(module.ladderPath);
  if (entities == NULL) {
    Cg_Warn("Failed to load practice ladder %s\n", module.ladderPath);
    return;
  }

  module.ladder = calloc(entities->count, sizeof(CGamePracticeEntry));
  assert(module.ladder);

  for (ListNode *node = entities->head; node; node = node->next) {
    const Entity *entity = node->element;

    CGamePracticeEntry *entry = &module.ladder[module.ladderLength];

    entry->tier = Cg_Practice_Integer(entity, "tier");
    Str_Copy(entry->name, cgi.EntityValue(entity, "name")->string, sizeof(entry->name));
    Str_Copy(entry->ai, cgi.EntityValue(entity, "ai")->string, sizeof(entry->ai));
    entry->fragLimit = Cg_Practice_Integer(entity, "frag_limit");
    entry->timeLimit = Cg_Practice_Integer(entity, "time_limit");

    if (entry->tier < 1 || !*entry->name || !*entry->ai) {
      Cg_Warn("Skipping %s entry with no tier, map or AI players\n", module.ladderPath);
      continue;
    }

    char ai[sizeof(entry->ai)];
    Str_Copy(ai, entry->ai, sizeof(ai));

    int32_t clients = 1;
    for (const char *name = strtok(ai, " \t\n"); name; name = strtok(NULL, " \t\n")) {
      clients++;
    }

    module.maxClients = Maxi(module.maxClients, clients);
    module.ladderLength++;
  }

  Cg_Practice_FreeEntities(entities);
}

/**
 * @brief Loads the entries the player has cleared.
 */
static void Cg_Practice_LoadProgress(void) {

  free(module.clears);
  module.clears = NULL;
  module.numClears = 0;

  List *entities = Cg_Practice_LoadEntities(module.progressPath);
  if (entities == NULL) {
    return;
  }

  module.clears = calloc(entities->count, sizeof(CGamePracticeClear));
  assert(module.clears);

  for (ListNode *node = entities->head; node; node = node->next) {
    const Entity *entity = node->element;

    CGamePracticeClear *clear = &module.clears[module.numClears++];

    Str_Copy(clear->gameplay, cgi.EntityValue(entity, "gameplay")->string, sizeof(clear->gameplay));
    Str_Copy(clear->name, cgi.EntityValue(entity, "name")->string, sizeof(clear->name));
    Str_Copy(clear->ai, cgi.EntityValue(entity, "ai")->string, sizeof(clear->ai));
  }

  Cg_Practice_FreeEntities(entities);
}

/**
 * @brief Writes the entries the player has cleared.
 */
static void Cg_Practice_WriteProgress(void) {

  File *file = cgi.OpenFileWrite(module.progressPath);
  if (file == NULL) {
    Cg_Warn("Failed to open %s\n", module.progressPath);
    return;
  }

  String *string = str("");
  assert(string);

  for (size_t i = 0; i < module.numClears; i++) {
    const CGamePracticeClear *clear = &module.clears[i];
    $(string, appendFormat, "{\n\tgameplay %s\n\tname %s\n\tai \"%s\"\n}\n",
      clear->gameplay, clear->name, clear->ai);
  }

  if (cgi.WriteFile(file, string->chars, string->length, 1) == -1) {
    Cg_Warn("Failed to write %s\n", module.progressPath);
  }

  release(string);
  cgi.CloseFile(file);
}

/**
 * @brief Loads the ladder that `cg_practiceLadder` names, and the player's progress through it,
 * if they are not loaded yet.
 */
static void Cg_Practice_Update(void) {

  if (!cg_practiceLadder->modified) {
    return;
  }

  cg_practiceLadder->modified = false;

  snprintf(module.ladderPath, sizeof(module.ladderPath), PRACTICE_LADDER, cg_practiceLadder->string);
  snprintf(module.progressPath, sizeof(module.progressPath), PRACTICE_PROGRESS, cg_practiceLadder->string);

  Cg_Practice_LoadLadder();
  Cg_Practice_LoadProgress();
}

/**
 * @brief The name of the gameplay with `id`, or `NULL` if the module has none.
 */
static const char *Cg_Practice_GamePlayName(int32_t id) {

  size_t count;
  const GamePlay *gamePlays = Cg_ListGamePlays(&count);

  for (size_t i = 0; i < count; i++) {
    if ((int32_t) gamePlays[i].id == id) {
      return gamePlays[i].name;
    }
  }

  return NULL;
}

/**
 * @brief The gameplay the server is playing, by name.
 */
static const char *Cg_Practice_CurrentGamePlay(void) {

  const char *name = Cg_Practice_GamePlayName((int32_t) strtol(cgi.ConfigString(CS_GAMEPLAY), NULL, 10));
  return name ?: "";
}

/**
 * @brief Practices `entry` under `gameplay`, on a new local server.
 */
void Cg_Practice_Start(const CGamePracticeEntry *entry, const char *gameplay) {

  File *file = cgi.OpenFileWrite(MAP_LIST_UI);
  if (file == NULL) {
    Cg_Warn("Failed to open %s\n", MAP_LIST_UI);
    return;
  }

  String *string = str("{\n\tname %s\n\tai \"%s\"\n\tgameplay %s\n\tpractice 1\n",
                       entry->name, entry->ai, gameplay);
  assert(string);

  if (entry->fragLimit > -1) {
    $(string, appendFormat, "\tfrag_limit %d\n", entry->fragLimit);
  }

  if (entry->timeLimit > -1) {
    $(string, appendFormat, "\ttime_limit %d\n", entry->timeLimit);
  }

  $(string, appendCharacters, "}\n");

  const int64_t written = cgi.WriteFile(file, string->chars, string->length, 1);

  release(string);
  cgi.CloseFile(file);

  if (written == -1) {
    Cg_Warn("Failed to write %s\n", MAP_LIST_UI);
    return;
  }

  static const char *levelCvars[] = {
    "g_fragLimit", "g_timeLimit", "g_captureLimit", "g_gravity", "g_gameplay"
  };

  for (size_t i = 0; i < lengthof(levelCvars); i++) {
    cgi.SetCvarString(levelCvars[i], "default");
  }

  cgi.SetCvarInteger("sv_public", 0);
  cgi.SetCvarInteger("sv_maxClients", module.maxClients);
  cgi.SetCvarString("sv_mapList", MAP_LIST_UI);
  cgi.SetCvarInteger(cg_practice->name, (int32_t) (entry - module.ladder));

  cgi.Cbuf("nextMap\n");
}

/**
 * @brief The ladder, in file order.
 */
const CGamePracticeEntry *Cg_Practice_Ladder(size_t *count) {

  Cg_Practice_Update();

  *count = module.ladderLength;
  return module.ladder;
}

/**
 * @brief True if the player has cleared `entry` under `gameplay`.
 */
bool Cg_Practice_IsCleared(const CGamePracticeEntry *entry, const char *gameplay) {

  Cg_Practice_Update();

  for (size_t i = 0; i < module.numClears; i++) {
    const CGamePracticeClear *clear = &module.clears[i];

    if (!Str_Compare(clear->gameplay, gameplay) &&
        !Str_Compare(clear->name, entry->name) &&
        !Str_Compare(clear->ai, entry->ai)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief The entry this practice match plays, or `NULL` if this is not a practice match from
 * the ladder.
 */
const CGamePracticeEntry *Cg_Practice_Current(void) {

  if (!cgameState.nextMap.practice) {
    return NULL;
  }

  Cg_Practice_Update();

  const int32_t index = cg_practice->integer;
  if (index < 0 || (size_t) index >= module.ladderLength) {
    return NULL;
  }

  char name[MAX_QPATH];
  StripExtension(Basename(cgi.ConfigString(CS_BSP)), name);

  const CGamePracticeEntry *entry = &module.ladder[index];
  return Str_Compare(entry->name, name) ? NULL : entry;
}

/**
 * @brief The entry after this practice match's, or `NULL` if there is none.
 */
const CGamePracticeEntry *Cg_Practice_Next(void) {

  const CGamePracticeEntry *entry = Cg_Practice_Current();
  if (entry == NULL || entry + 1 >= module.ladder + module.ladderLength) {
    return NULL;
  }

  return entry + 1;
}

/**
 * @brief True if the local player holds the top score alone.
 */
bool Cg_Practice_Won(void) {

  size_t count;
  const GameScore *scores = Cg_Scores(&count);

  const GameScore *self = NULL;
  int32_t best = INT32_MIN;

  for (size_t i = 0; i < count; i++) {
    const GameScore *score = &scores[i];

    if (score->flags & (SCORE_SPECTATOR | SCORE_AGGREGATE)) {
      continue;
    }

    if (score->client == cgi.client->frame.ps.client) {
      self = score;
    } else {
      best = Maxi(best, score->score);
    }
  }

  return self && self->score > best;
}

/**
 * @brief Records a win of this practice match, if the player has not cleared it before.
 */
void Cg_Practice_RecordWin(void) {

  const CGamePracticeEntry *entry = Cg_Practice_Current();
  if (entry == NULL) {
    return;
  }

  const char *gameplay = Cg_Practice_CurrentGamePlay();
  if (Cg_Practice_IsCleared(entry, gameplay)) {
    return;
  }

  module.clears = realloc(module.clears, (module.numClears + 1) * sizeof(CGamePracticeClear));
  assert(module.clears);

  CGamePracticeClear *clear = &module.clears[module.numClears++];

  Str_Copy(clear->gameplay, gameplay, sizeof(clear->gameplay));
  Str_Copy(clear->name, entry->name, sizeof(clear->name));
  Str_Copy(clear->ai, entry->ai, sizeof(clear->ai));

  Cg_Practice_WriteProgress();
}

/**
 * @brief Plays the entry after this practice match's, under the same gameplay, if the player
 * won this one and there is a next.
 * @return True if the next entry starts.
 */
bool Cg_Practice_PlayNext(void) {

  const CGamePracticeEntry *next = Cg_Practice_Won() ? Cg_Practice_Next() : NULL;
  if (next == NULL) {
    return false;
  }

  Cg_Practice_Start(next, Cg_Practice_CurrentGamePlay());
  return true;
}

/**
 * @brief Plays this practice match again, from the map list that started it.
 */
void Cg_Practice_Retry(void) {
  cgi.Cbuf("nextMap\n");
}

/**
 * @brief Ends this practice match.
 */
void Cg_Practice_Exit(void) {

  cgi.SetCvarInteger(cg_practice->name, -1);
  cgi.SetCvarString("sv_mapList", "maps.lst");
  cgi.Cbuf("disconnect\n");
}

/**
 * @brief Plays the entry of the `cg_practiceLadder` ladder with the number given, from 1, under
 * the gameplay given, or deathmatch.
 */
static void Cg_Practice_f(void) {

  Cg_Practice_Update();

  if (!*cgi.Argv(1)) {
    for (size_t i = 0; i < module.ladderLength; i++) {
      const CGamePracticeEntry *entry = &module.ladder[i];
      cgi.Print("%2zu  tier %d  %-12s %s\n", i + 1, entry->tier, entry->name, entry->ai);
    }
    cgi.Print("Usage: practice <number> [gameplay]\n");
    return;
  }

  const int32_t index = (int32_t) strtol(cgi.Argv(1), NULL, 10) - 1;
  if (index < 0 || (size_t) index >= module.ladderLength) {
    cgi.Print("No ladder entry %s\n", cgi.Argv(1));
    return;
  }

  Cg_Practice_Start(&module.ladder[index], *cgi.Argv(2) ? cgi.Argv(2) : "deathmatch");
}

/**
 * @brief Loads the ladder and the player's progress, and adds the practice command and cvars.
 */
void Cg_Practice_Init(void) {

  cg_practice = cgi.AddCvar("cg_practice", "-1", 0,
                            "The ladder entry this practice match plays, or -1 for none");

  cg_practiceLadder = cgi.AddCvar("cg_practiceLadder", "default", CVAR_ARCHIVE,
                                  "The ladder that practice matches come from, by name");
  cg_practiceLadder->modified = true;

  cgi.AddCmd("practice", Cg_Practice_f, CMD_CGAME, "Practice against AI players: practice <number> [gameplay]");

  Cg_Practice_Update();
}

/**
 * @brief Frees the ladder and the player's progress.
 */
void Cg_Practice_Shutdown(void) {

  free(module.ladder);
  free(module.clears);

  memset(&module, 0, sizeof(module));
}
