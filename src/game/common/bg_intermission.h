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
 * @brief What the intermission is offering, shared by the game and the client game so that
 * the screen that shows the maps and the server that serves one agree.
 */

/**
 * @brief The most maps an intermission offers, which is what the number keys reach.
 */
#define MAX_NEXT_MAPS 4

/**
 * @brief The `CS_NEXT_MAP` config string: empty outside the intermission, else these
 * fields separated by `\`.
 * @details The first field says whether ballots are being taken. The rest are one
 * `name`, `votes` pair per candidate, so the count of candidates is what the string
 * carries rather than something both sides have to agree on in advance.
 */
typedef enum {
  NEXT_MAP_CS_VOTING, // "1" while ballots are being taken, else "0"
  NEXT_MAP_CS_MAPS, // the first name; from here the fields alternate name, votes
  NEXT_MAP_CS_FIELDS
} next_map_cs_field_t;

/**
 * @brief The number of `\`-separated fields a `CS_NEXT_MAP` string with `count` maps has.
 */
#define NEXT_MAP_CS_FIELDS_FOR(count) (NEXT_MAP_CS_MAPS + (count) * 2)
