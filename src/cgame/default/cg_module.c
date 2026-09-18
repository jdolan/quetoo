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


#include "cg_local.h"

#include "MapListCollectionItemView.h"

static struct {
  FilterCreateServerMapList FilterCreateServerMapList;
} previous;

/**
 * @brief The games this module plays as variants of deathmatch, which the maps name
 * rather than `default`.
 */
static const char *cg_default_games[] = { "dm", "tdm", "duel", "instagib" };

/**
 * @brief Lists a map made for any of the deathmatch variants.
 */
static bool Cg_FilterCreateServerMapList_Default(const MapListItemInfo *info) {

  for (size_t i = 0; i < lengthof(cg_default_games); i++) {
    if (q_str_has_token(info->games, cg_default_games[i])) {
      return true;
    }
  }

  return previous.FilterCreateServerMapList(info);
}

/**
 * @brief Plain deathmatch draws no element and adds no effect of its own, and
 * switches on none of the optional features. Its one hook names the deathmatch
 * variants in the create-server map browser, since no map is made for `default`.
 */
void Cg_Module_Init(void) {
  static bool installed;

  if (installed) {
    return;
  }

  previous.FilterCreateServerMapList = Cg_FilterCreateServerMapList;
  Cg_FilterCreateServerMapList = Cg_FilterCreateServerMapList_Default;

  installed = true;
}

/**
 * @brief Plain deathmatch holds nothing of its own to release.
 */
void Cg_Module_Shutdown(void) {
}
