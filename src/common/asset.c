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

#include "asset.h"

/**
 * @brief Prepends the context prefix to a name if not already present.
 */
void Asset_Path(const char *name, char *out, size_t len, AssetContext context) {

  *out = '\0';

  switch (context) {
    case ASSET_CONTEXT_NONE:
      break;
    case ASSET_CONTEXT_TEXTURES:
      if (Str_CompareN(name, "textures/", sizeof("textures/") - 1)) {
        Str_Append(out, "textures/", len);
      }
      break;
    case ASSET_CONTEXT_MODELS:
      if (Str_CompareN(name, "models/", sizeof("models/") - 1)) {
        Str_Append(out, "models/", len);
      }
      break;
    case ASSET_CONTEXT_PLAYERS:
      if (Str_CompareN(name, "players/", sizeof("players/") - 1)) {
        Str_Append(out, "players/", len);
      }
      break;
    case ASSET_CONTEXT_SPRITES:
      if (Str_CompareN(name, "sprites/", sizeof("sprites/") - 1)) {
        Str_Append(out, "sprites/", len);
      }
      break;
    case ASSET_CONTEXT_SOUNDS:
      if (Str_CompareN(name, "sounds/", sizeof("sounds/") - 1)) {
        Str_Append(out, "sounds/", len);
      }
      break;
    case ASSET_CONTEXT_UI:
      if (Str_CompareN(name, "ui/", sizeof("ui/") - 1)) {
        Str_Append(out, "ui/", len);
      }
      break;
  }

  Str_Append(out, name, len);
}
