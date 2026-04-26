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

#include "g_local.h"

static cvar_t *ai_name_prefix;

/**
 * @brief The static roster of bot definitions.
 * Each entry defines a unique bot with its own name, appearance, and personality.
 */
static const g_ai_roster_t g_ai_roster[] = {
  // name          skin                skill  aggr   aware
  { "Stroggo",    "qforcer/default",   .50f,  .50f,  .50f },
  { "Enforcer",   "guard/default",     .65f,  .60f,  .55f },
  { "Berserker",  "gork/default",      .40f,  .85f,  .30f },
  { "Gunner",     "guard/mgss",        .70f,  .45f,  .70f },
  { "Gladiator",  "dragoon/default",   .55f,  .70f,  .45f },
  { "Makron",     "dragoon/baron",     .85f,  .55f,  .80f },
  { "Brain",      "gork/ctf",          .75f,  .25f,  .90f },
  { "Widow",      "bunker/default",    .60f,  .40f,  .65f },
  { "Tank",       "bunker/hax",        .35f,  .80f,  .35f },
  { "Medic",      "guard/sggrd",       .45f,  .30f,  .75f },
  { "Parasite",   "dragoon/bastard",   .80f,  .75f,  .60f },
  { "Flyer",      "bunker/fidget",     .55f,  .65f,  .40f },
};

static const uint32_t ai_roster_count = lengthof(g_ai_roster);

/**
 * @brief Round-robin index for assigning bots from the roster.
 */
static uint32_t ai_roster_index;
static uint32_t ai_roster_suffix;

/**
 * @brief Create the user info for the specified bot and return its roster entry.
 */
const g_ai_roster_t *G_Ai_GetUserInfo(const g_client_t *cl, char *info) {

  const g_ai_roster_t *entry = &g_ai_roster[ai_roster_index];

  g_strlcpy(info, DEFAULT_BOT_INFO, MAX_INFO_STRING_STRING);

  InfoString_Set(info, "skin", entry->skin);
  InfoString_Set(info, "color", va("%u", RandomRangeu(0, 360)));
  InfoString_Set(info, "hand", va("%u", RandomRangeu(0, 3)));
  InfoString_Set(info, "head", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));
  InfoString_Set(info, "shirt", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));
  InfoString_Set(info, "pants", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));

  if (ai_roster_suffix == 0) {
    InfoString_Set(info, "name", va("%s%s", ai_name_prefix->string, entry->name));
  } else {
    InfoString_Set(info, "name", va("%s%s %i", ai_name_prefix->string, entry->name, ai_roster_suffix + 1));
  }

  ai_roster_index++;

  if (ai_roster_index == ai_roster_count) {
    ai_roster_index = 0;
    ai_roster_suffix++;
  }

  return entry;
}

/**
 * @brief Initializes the AI name prefix console variable.
 */
void G_Ai_InitSkins(void) {

  ai_name_prefix = gi.AddCvar("ai_name_prefix", "^0[^1BOT^0] ^7", 0, NULL);
}

/**
 * @brief Shuts down AI skin and name resources.
 */
void G_Ai_ShutdownSkins(void) {
}
