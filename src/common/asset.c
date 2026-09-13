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

#include "asset.h"

/**
 * @brief Prepends the context prefix to a name if not already present.
 */
void Asset_Path(const char *name, char *out, size_t len, asset_context_t context) {

  *out = '\0';

  switch (context) {
    case ASSET_CONTEXT_NONE:
      break;
    case ASSET_CONTEXT_TEXTURES:
      if (q_strncmp(name, "textures/", sizeof("textures/") - 1)) {
        q_strlcat(out, "textures/", len);
      }
      break;
    case ASSET_CONTEXT_MODELS:
      if (q_strncmp(name, "models/", sizeof("models/") - 1)) {
        q_strlcat(out, "models/", len);
      }
      break;
    case ASSET_CONTEXT_PLAYERS:
      if (q_strncmp(name, "players/", sizeof("players/") - 1)) {
        q_strlcat(out, "players/", len);
      }
      break;
    case ASSET_CONTEXT_SPRITES:
      if (q_strncmp(name, "sprites/", sizeof("sprites/") - 1)) {
        q_strlcat(out, "sprites/", len);
      }
      break;
    case ASSET_CONTEXT_SOUNDS:
      if (q_strncmp(name, "sounds/", sizeof("sounds/") - 1)) {
        q_strlcat(out, "sounds/", len);
      }
      break;
    case ASSET_CONTEXT_UI:
      if (q_strncmp(name, "ui/", sizeof("ui/") - 1)) {
        q_strlcat(out, "ui/", len);
      }
      break;
  }

  q_strlcat(out, name, len);
}
