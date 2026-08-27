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

#include "shared/shared.h"

/**
 * @file
 * @brief What a vote is about, shared by the game and the client game so that
 * the screen that calls a vote and the server that runs it agree.
 */

/**
 * @brief The kind of argument a vote type takes.
 */
typedef enum {
  VOTE_ARG_NONE,
  VOTE_ARG_MAP, // a map name, or "next"
  VOTE_ARG_CLIENT, // a client's name
  VOTE_ARG_INTEGER // a number within the type's range
} vote_arg_t;

/**
 * @brief A kind of vote a client may call.
 */
typedef struct {

  /**
   * @brief The name, as the `vote` command and the config string spell it.
   */
  const char *name;

  /**
   * @brief The title the client game shows.
   */
  const char *title;

  /**
   * @brief What the argument is.
   */
  vote_arg_t arg;

  /**
   * @brief The range of an integer argument, inclusive.
   */
  int32_t min, max;
} vote_type_t;

/**
 * @brief The votes every game offers.
 */
static const vote_type_t vote_types_common[] = {
  { "map", "Change map", VOTE_ARG_MAP, 0, 0 },
  { "bots", "Bots", VOTE_ARG_INTEGER, 0, 8 },
  { "spectate", "Force spectate", VOTE_ARG_CLIENT, 0, 0 },
  { "frag_limit", "Frag limit", VOTE_ARG_INTEGER, 0, 100 },
  { "time_limit", "Time limit", VOTE_ARG_INTEGER, 0, 60 },
};

/**
 * @brief The `CS_VOTE` config string: empty when no vote is active, else these
 * fields separated by `\`, in this order.
 */
typedef enum {
  VOTE_CS_TYPE,
  VOTE_CS_ARG,
  VOTE_CS_YES,
  VOTE_CS_NO,
  VOTE_CS_ELIGIBLE,
  VOTE_CS_DEADLINE, // level time in milliseconds
  VOTE_CS_INITIATOR,
  VOTE_CS_FIELDS
} vote_cs_field_t;
