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
#include "ui/play/MapListCollectionItemView.h"

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
  char startedLadder[MAX_QPATH];

  CGamePracticeEntry *ladder;
  size_t ladderLength;
  uint32_t ladderGeneration;
  int32_t maxClients;

  CGamePracticeClear *clears;
  size_t numClears;

  int32_t result;
  uint32_t resultScores;

  CGamePracticeLadder *ladders;
  size_t numLadders;

  ClientKeyDest keyDest;
  bool menuPaused;
} module;

/**
 * @brief The integer `key` holds in `entity`, or -1 if it holds none.
 */
static int32_t Cg_Practice_Integer(const Entity *entity, const char *key) {

  const Entity *value = cgi.EntityValue(entity, key);
  return (value->parsed & ENTITY_INTEGER) ? value->integer : -1;
}

/**
 * @brief The ladder named `name`, or `NULL` if the search path holds none.
 */
static const CGamePracticeLadder *Cg_Practice_FindLadder(const char *name) {

  for (size_t i = 0; i < module.numLadders; i++) {
    if (!Str_Compare(module.ladders[i].name, name)) {
      return &module.ladders[i];
    }
  }

  return NULL;
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
 * @brief Loads the ladder. The `title` entity, an entry without a map or AI players, an entry
 * with no tier or a tier above `PRACTICE_MAX_TIERS`, an entry past its tier's
 * `PRACTICE_MAX_TIER_ENTRIES`, and an entry with more than `PRACTICE_MAX_AI` AI players, are
 * skipped.
 */
static void Cg_Practice_LoadLadder(void) {

  free(module.ladder);
  module.ladder = NULL;
  module.ladderLength = 0;
  module.maxClients = 0;
  module.ladderGeneration++;

  const CGamePracticeLadder *ladder = Cg_Practice_FindLadder(cg_practiceLadder->string);

  List *entities = Cg_Practice_LoadEntities(module.ladderPath);
  if (entities == NULL) {
    Cg_Warn("Failed to load practice ladder %s\n", module.ladderPath);
    return;
  }

  module.ladder = calloc(entities->count, sizeof(CGamePracticeEntry));
  assert(module.ladder);

  size_t tierEntries[PRACTICE_MAX_TIERS] = { 0 };

  for (ListNode *node = entities->head; node; node = node->next) {
    const Entity *entity = node->element;

    if (*cgi.EntityValue(entity, "title")->string) {
      continue;
    }

    CGamePracticeEntry *entry = &module.ladder[module.ladderLength];

    entry->tier = Cg_Practice_Integer(entity, "tier");
    Str_Copy(entry->name, cgi.EntityValue(entity, "name")->string, sizeof(entry->name));
    Str_Copy(entry->ai, cgi.EntityValue(entity, "ai")->string, sizeof(entry->ai));
    entry->fragLimit = Cg_Practice_Integer(entity, "frag_limit");
    entry->captureLimit = Cg_Practice_Integer(entity, "capture_limit");
    entry->timeLimit = Cg_Practice_Integer(entity, "time_limit");
    entry->numTeams = Cg_Practice_Integer(entity, "num_teams");

    if (entry->numTeams == -1 && ladder) {
      entry->numTeams = ladder->numTeams;
    }

    if (entry->tier < 1 || !*entry->name || !*entry->ai) {
      Cg_Warn("Skipping %s entry with no tier, map or AI players\n", module.ladderPath);
      continue;
    }

    if (entry->tier > PRACTICE_MAX_TIERS) {
      Cg_Warn("Skipping %s in tier %d: a ladder has at most %d tiers\n",
              entry->name, entry->tier, PRACTICE_MAX_TIERS);
      continue;
    }

    if (tierEntries[entry->tier - 1] == PRACTICE_MAX_TIER_ENTRIES) {
      Cg_Warn("Skipping %s in tier %d: a tier has at most %d entries\n",
              entry->name, entry->tier, PRACTICE_MAX_TIER_ENTRIES);
      continue;
    }

    char ai[sizeof(entry->ai)];
    Str_Copy(ai, entry->ai, sizeof(ai));

    int32_t numAi = 0;
    for (const char *name = strtok(ai, " \t\n"); name; name = strtok(NULL, " \t\n")) {
      numAi++;
    }

    if (numAi > PRACTICE_MAX_AI) {
      Cg_Warn("Skipping %s in tier %d: an entry has at most %d AI players\n",
              entry->name, entry->tier, PRACTICE_MAX_AI);
      continue;
    }

    tierEntries[entry->tier - 1]++;

    module.maxClients = Maxi(module.maxClients, numAi + 1);
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

  if (cgi.WriteFile(file, string->chars, string->length, 1) != 1) {
    Cg_Warn("Failed to write %s\n", module.progressPath);
  }

  release(string);
  cgi.CloseFile(file);
}

/**
 * @brief The gameplay named `name`, or `NULL` if the module has none.
 */
static const GamePlay *Cg_Practice_GamePlay(const char *name) {

  size_t count;
  const GamePlay *gamePlays = Cg_ListGamePlays(&count);

  for (size_t i = 0; i < count; i++) {
    if (!Str_Compare(gamePlays[i].name, name)) {
      return &gamePlays[i];
    }
  }

  return NULL;
}

/**
 * @return True if the module offers a gameplay with teams, if `teams`, or else one without.
 */
static bool Cg_Practice_OffersChoice(bool teams) {

  size_t count;
  const GamePlay *gamePlays = Cg_ListGamePlays(&count);

  for (size_t i = 0; i < count; i++) {
    if (((gamePlays[i].id & GAMEPLAY_TEAMS) != 0) == teams) {
      return true;
    }
  }

  return false;
}

/**
 * @brief `EnumerateFiles` function that adds the ladder at `path` to the ladders. A ladder made
 * for games that the module does not play, by its `games` as a map's worldspawn names them, or that
 * sets a gameplay the module does not offer, belongs to another module, and is skipped. So is a
 * ladder that leaves the gameplay to the player, if the module offers no gameplay to choose, and
 * a ladder that sets teams and a gameplay without them.
 */
static void Cg_Practice_LoadLadders_enumerate(const char *path, void *data) {

  char name[MAX_QPATH];
  StripExtension(Basename(path), name);

  const char *prefix = "practice-";
  if (strncmp(name, prefix, strlen(prefix)) || !name[strlen(prefix)]) {
    return;
  }

  for (size_t i = 0; i < module.numLadders; i++) {
    if (!Str_Compare(module.ladders[i].name, name + strlen(prefix))) {
      return;
    }
  }

  CGamePracticeLadder ladder = { .numTeams = -1 };
  MapListItemInfo info = { .games = "dm" };

  Str_Copy(ladder.name, name + strlen(prefix), sizeof(ladder.name));
  Str_Copy(ladder.title, ladder.name, sizeof(ladder.title));

  List *entities = Cg_Practice_LoadEntities(path);
  if (entities) {
    for (ListNode *node = entities->head; node; node = node->next) {
      const char *title = cgi.EntityValue(node->element, "title")->string;
      if (*title) {
        Str_Copy(ladder.title, title, sizeof(ladder.title));
        ladder.numTeams = Cg_Practice_Integer(node->element, "num_teams");

        const char *games = cgi.EntityValue(node->element, "games")->string;
        if (*games) {
          Str_Copy(info.games, games, sizeof(info.games));
        }

        const char *gameplay = cgi.EntityValue(node->element, "gameplay")->string;
        if (*gameplay) {
          ladder.gamePlay = Cg_Practice_GamePlay(gameplay);
          if (ladder.gamePlay == NULL) {
            Cg_Debug("Skipping %s: no gameplay %s\n", path, gameplay);
            Cg_Practice_FreeEntities(entities);
            return;
          }

          if (ladder.numTeams > -1 && !(ladder.gamePlay->id & GAMEPLAY_TEAMS)) {
            Cg_Warn("Skipping %s: %s has no teams\n", path, gameplay);
            Cg_Practice_FreeEntities(entities);
            return;
          }
        }
        break;
      }
    }

    Cg_Practice_FreeEntities(entities);
  }

  if (!GameInfo_HasMap(&cgameGameInfo, info.games)) {
    Cg_Debug("Skipping %s: made for %s\n", path, info.games);
    return;
  }

  if (ladder.gamePlay == NULL && !Cg_Practice_OffersChoice(ladder.numTeams > -1)) {
    Cg_Debug("Skipping %s: no gameplay to choose\n", path);
    return;
  }

  module.ladders = realloc(module.ladders, (module.numLadders + 1) * sizeof(CGamePracticeLadder));
  assert(module.ladders);

  module.ladders[module.numLadders++] = ladder;
}

/**
 * @brief Sorts the ladders by title.
 */
static int32_t Cg_Practice_LoadLadders_compare(const void *a, const void *b) {

  const CGamePracticeLadder *la = a, *lb = b;

  return Str_Compare(la->title, lb->title);
}

/**
 * @brief Finds every ladder that the search path holds.
 */
static void Cg_Practice_LoadLadders(void) {

  cgi.EnumerateFiles("/practice-*.lst", Cg_Practice_LoadLadders_enumerate, NULL);

  qsort(module.ladders, module.numLadders, sizeof(CGamePracticeLadder), Cg_Practice_LoadLadders_compare);
}

/**
 * @brief Loads the ladder that `cg_practiceLadder` names, and the player's progress through it,
 * and loads them again when `cg_practiceLadder` changes.
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
 * @brief Practices `entry` under `gameplay`, or under the gameplay its ladder sets, on a new
 * local server.
 */
void Cg_Practice_Start(const CGamePracticeEntry *entry, const char *gameplay) {

  const GamePlay *ladderGamePlay = Cg_Practice_LadderGamePlay();
  if (ladderGamePlay) {
    gameplay = ladderGamePlay->name;
  }

  File *file = cgi.OpenFileWrite(MAP_LIST_UI);
  if (file == NULL) {
    Cg_Warn("Failed to open %s\n", MAP_LIST_UI);
    return;
  }

  String *string = str("{\n\tname %s\n\tai \"%s\"\n\tgameplay %s\n",
                       entry->name, entry->ai, gameplay);
  assert(string);

  if (entry->fragLimit > -1) {
    $(string, appendFormat, "\tfrag_limit %d\n", entry->fragLimit);
  }

  if (entry->captureLimit > -1) {
    $(string, appendFormat, "\tcapture_limit %d\n", entry->captureLimit);
  }

  if (entry->timeLimit > -1) {
    $(string, appendFormat, "\ttime_limit %d\n", entry->timeLimit);
  }

  if (entry->numTeams > -1) {
    $(string, appendFormat, "\tnum_teams %d\n", entry->numTeams);
  }

  $(string, appendCharacters, "}\n");

  const int64_t written = cgi.WriteFile(file, string->chars, string->length, 1);

  release(string);
  cgi.CloseFile(file);

  if (written != 1) {
    Cg_Warn("Failed to write %s\n", MAP_LIST_UI);
    return;
  }

  static const char *levelCvars[] = {
    "g_fragLimit", "g_timeLimit", "g_captureLimit", "g_numTeams", "g_gravity", "g_gameplay"
  };

  for (size_t i = 0; i < lengthof(levelCvars); i++) {
    cgi.SetCvarString(levelCvars[i], "default");
  }

  cgi.SetCvarInteger("sv_practice", 1);
  cgi.SetCvarInteger("sv_maxClients", module.maxClients);
  cgi.SetCvarString("sv_mapList", MAP_LIST_UI);
  cgi.SetCvarInteger(cg_practice->name, (int32_t) (entry - module.ladder));
  Str_Copy(module.startedLadder, cg_practiceLadder->string, sizeof(module.startedLadder));

  cgi.Cbuf("nextMap\n");
}

/**
 * @brief Every ladder that the search path holds, by title.
 */
const CGamePracticeLadder *Cg_Practice_Ladders(size_t *count) {

  *count = module.numLadders;
  return module.ladders;
}

/**
 * @brief The gameplay that the `cg_practiceLadder` ladder sets, or `NULL` to let the player
 * choose.
 */
const GamePlay *Cg_Practice_LadderGamePlay(void) {

  const CGamePracticeLadder *ladder = Cg_Practice_FindLadder(cg_practiceLadder->string);
  return ladder ? ladder->gamePlay : NULL;
}

/**
 * @return True if the ladder offers `gamePlay`: it is the ladder's gameplay, or the ladder sets
 * none and `gamePlay` has teams if and only if the ladder does.
 */
bool Cg_Practice_OffersGamePlay(const GamePlay *gamePlay) {

  const CGamePracticeLadder *ladder = Cg_Practice_FindLadder(cg_practiceLadder->string);
  if (ladder == NULL) {
    return false;
  }

  if (ladder->gamePlay) {
    return gamePlay == ladder->gamePlay;
  }

  return ((gamePlay->id & GAMEPLAY_TEAMS) != 0) == (ladder->numTeams > -1);
}

/**
 * @return The first gameplay that the ladder offers, or `NULL` if it offers none.
 */
const GamePlay *Cg_Practice_DefaultGamePlay(void) {

  size_t count;
  const GamePlay *gamePlays = Cg_ListGamePlays(&count);

  for (size_t i = 0; i < count; i++) {
    if (Cg_Practice_OffersGamePlay(&gamePlays[i])) {
      return &gamePlays[i];
    }
  }

  return NULL;
}

/**
 * @brief The number of times that the ladder loaded, which changes each time that the entries
 * that `Cg_Practice_Ladder` returns move.
 */
uint32_t Cg_Practice_LadderGeneration(void) {

  Cg_Practice_Update();

  return module.ladderGeneration;
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
 * the ladder, or if `cg_practiceLadder` changed since it started.
 */
const CGamePracticeEntry *Cg_Practice_Current(void) {

  if (!cgameState.nextMap.practice) {
    return NULL;
  }

  Cg_Practice_Update();

  if (Str_Compare(module.startedLadder, cg_practiceLadder->string)) {
    return NULL;
  }

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
 * @brief The points that decide a team game: captures where the module has flags, else frags.
 */
static int32_t Cg_Practice_TeamPoints(const GameScore *score) {
#if defined(G_CTF)
  return score->captures;
#else
  return score->frags;
#endif
}

/**
 * @brief True if the team with index `team` holds the top team score alone.
 */
static bool Cg_Practice_TeamWon(int32_t team) {

  size_t count;
  const GameScore *scores = Cg_TeamScores(&count);

  int32_t self = INT32_MIN, best = INT32_MIN;

  for (size_t i = 0; i < count; i++) {
    const GameScore *score = &scores[i];

    if ((score->flags & ~SCORE_AGGREGATE) == team) {
      self = Cg_Practice_TeamPoints(score);
    } else {
      best = Maxi(best, Cg_Practice_TeamPoints(score));
    }
  }

  return self > best;
}

/**
 * @brief True if the local player, or the local player's team, holds the top score alone.
 */
static bool Cg_Practice_Won(void) {

  size_t count;
  const GameScore *scores = Cg_Scores(&count);

  for (size_t i = 0; i < count; i++) {
    if (scores[i].client == cgi.client->frame.ps.client && scores[i].team) {
      return Cg_Practice_TeamWon(scores[i].team - 1);
    }
  }

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
static void Cg_Practice_RecordWin(void) {

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
 * @brief Waits for the result of this practice match, as its intermission begins.
 */
void Cg_Practice_ResetResult(void) {

  module.result = -1;
  module.resultScores = Cg_ScoresGeneration();
}

/**
 * @brief The result of this practice match, which records a win once it is final.
 * @return 1 for a win, 0 for a loss, or -1 until the intermission's scores arrive.
 * @remarks The server shares one score cache for half a second, so the first scores of the
 * intermission can predate the frag that ended it. The second cannot.
 */
int32_t Cg_Practice_Result(void) {

  if (module.result == -1 && Cg_ScoresGeneration() - module.resultScores >= 2) {
    module.result = Cg_Practice_Won();

    if (module.result == 1) {
      Cg_Practice_RecordWin();
    }
  }

  return module.result;
}

/**
 * @brief Plays the entry after this practice match's, under the same gameplay, if the player
 * won this one and there is a next.
 * @return True if the next entry starts.
 */
bool Cg_Practice_PlayNext(void) {

  const CGamePracticeEntry *next = Cg_Practice_Result() == 1 ? Cg_Practice_Next() : NULL;
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
 * @brief Pauses a practice match while the menu is open, and resumes it when the menu closes.
 */
void Cg_Practice_CheckPause(void) {

  const ClientKeyDest keyDest = cgi.GetKeyDest();
  if (keyDest == module.keyDest) {
    return;
  }

  if (module.keyDest == KEY_GAME && keyDest == KEY_UI) {
    if (cgi.GetCvarInteger("sv_practice") && !cgi.GetCvarInteger("sv_paused")) {
      cgi.SetCvarInteger("sv_paused", 1);
      module.menuPaused = true;
    }
  } else if (keyDest == KEY_GAME && module.menuPaused) {
    cgi.SetCvarInteger("sv_paused", 0);
    module.menuPaused = false;
  }

  module.keyDest = keyDest;
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
 * @brief Plays the entry of the ladder named first with the number given, from 1, under the
 * gameplay given, or else the first that the ladder offers. The ladder becomes
 * `cg_practiceLadder`, so that the match counts toward its progress. With no number, lists the
 * ladder's entries, and with no ladder, lists the ladders.
 * @remarks The ladder is loaded through `cg_practiceLadder` to read its entries, and the cvar is
 * set back unless a match starts, so that a match in progress still counts toward its own ladder.
 */
static void Cg_Practice_f(void) {

  if (!*cgi.Argv(1)) {
    for (size_t i = 0; i < module.numLadders; i++) {
      cgi.Print("%-12s %s\n", module.ladders[i].name, module.ladders[i].title);
    }
    cgi.Print("Usage: practice <ladder> [number] [gameplay]\n");
    return;
  }

  if (Cg_Practice_FindLadder(cgi.Argv(1)) == NULL) {
    cgi.Print("No ladder %s\n", cgi.Argv(1));
    return;
  }

  char previous[MAX_QPATH];
  Str_Copy(previous, cg_practiceLadder->string, sizeof(previous));

  cgi.SetCvarString(cg_practiceLadder->name, cgi.Argv(1));
  Cg_Practice_Update();

  const int32_t index = (int32_t) strtol(cgi.Argv(2), NULL, 10) - 1;

  if (!*cgi.Argv(2)) {
    for (size_t i = 0; i < module.ladderLength; i++) {
      const CGamePracticeEntry *entry = &module.ladder[i];
      cgi.Print("%2zu  tier %d  %-12s %s\n", i + 1, entry->tier, entry->name, entry->ai);
    }
    cgi.Print("Usage: practice %s <number> [gameplay]\n", cgi.Argv(1));
  } else if (index < 0 || (size_t) index >= module.ladderLength) {
    cgi.Print("No entry %s in ladder %s\n", cgi.Argv(2), cgi.Argv(1));
  } else {
    const GamePlay *gamePlay = *cgi.Argv(3) ? Cg_Practice_GamePlay(cgi.Argv(3)) : Cg_Practice_DefaultGamePlay();
    if (gamePlay && Cg_Practice_OffersGamePlay(gamePlay)) {
      Cg_Practice_Start(&module.ladder[index], gamePlay->name);
      return;
    }
    cgi.Print("Ladder %s does not offer gameplay %s\n", cgi.Argv(1), *cgi.Argv(3) ? cgi.Argv(3) : "");
  }

  cgi.SetCvarString(cg_practiceLadder->name, previous);
  Cg_Practice_Update();
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

  module.result = -1;

  Cg_Practice_LoadLadders();

  if (module.numLadders && !Cg_Practice_FindLadder(cg_practiceLadder->string)) {
    const CGamePracticeLadder *ladder = Cg_Practice_FindLadder(cg_practiceLadder->defaultString);
    cgi.SetCvarString(cg_practiceLadder->name, (ladder ?: &module.ladders[0])->name);
  }

  cgi.AddCmd("practice", Cg_Practice_f, CMD_CGAME, "Practice against AI players: practice <ladder> [number] [gameplay]");

  Cg_Practice_Update();
}

/**
 * @brief Frees the ladder and the player's progress.
 */
void Cg_Practice_Shutdown(void) {

  free(module.ladder);
  free(module.clears);
  free(module.ladders);

  memset(&module, 0, sizeof(module));
}
