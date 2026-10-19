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

#include "cg_types.h"

/**
 * @file
 * @brief Practice: a ladder of matches against AI players, played on a local server.
 */

/**
 * @brief A ladder, by name: every practice match by tier, in the map list's entity format.
 */
#define PRACTICE_LADDER "practice-%s.lst"

/**
 * @brief The entries of a ladder, by name, that the player has cleared, written to the module's
 * user directory.
 */
#define PRACTICE_PROGRESS "progress-%s.lst"

/**
 * @brief The most tiers of a ladder.
 */
#define PRACTICE_MAX_TIERS 3

/**
 * @brief The most entries of a tier.
 */
#define PRACTICE_MAX_TIER_ENTRIES 4

/**
 * @brief A ladder that a `practice-<name>.lst` file holds.
 */
typedef struct {

  /**
   * @brief The name, which `cg_practiceLadder` takes.
   */
  char name[MAX_QPATH];

  /**
   * @brief The title that the file's `title` entity gives, or the name if it gives none.
   */
  char title[MAX_QPATH];

  /**
   * @brief The gameplay that the file's `title` entity sets, or `NULL` to let the player choose.
   */
  const GamePlay *gamePlay;
} CGamePracticeLadder;

/**
 * @brief One match of the ladder.
 */
typedef struct {
  /**
   * @brief The tier, 1 to `PRACTICE_MAX_TIERS`, which groups entries into rows.
   */
  int32_t tier;

  /**
   * @brief The map.
   */
  char name[MAX_QPATH];

  /**
   * @brief The AI players, by name and separated by spaces.
   */
  char ai[MAX_STRING_CHARS];

  /**
   * @brief The frag limit and the time limit in minutes, or -1 to leave them to the level.
   */
  int32_t fragLimit, timeLimit;
} CGamePracticeEntry;

#if defined(__CG_LOCAL_H__)
extern Cvar *cg_practice;
extern Cvar *cg_practiceLadder;

void Cg_Practice_Init(void);
void Cg_Practice_Shutdown(void);

const CGamePracticeLadder *Cg_Practice_Ladders(size_t *count);
const CGamePracticeEntry *Cg_Practice_Ladder(size_t *count);
const GamePlay *Cg_Practice_LadderGamePlay(void);
uint32_t Cg_Practice_LadderGeneration(void);
bool Cg_Practice_IsCleared(const CGamePracticeEntry *entry, const char *gameplay);

void Cg_Practice_Start(const CGamePracticeEntry *entry, const char *gameplay);
const CGamePracticeEntry *Cg_Practice_Current(void);
const CGamePracticeEntry *Cg_Practice_Next(void);
void Cg_Practice_ResetResult(void);
int32_t Cg_Practice_Result(void);

bool Cg_Practice_PlayNext(void);

void Cg_Practice_Retry(void);
void Cg_Practice_Exit(void);
#endif
