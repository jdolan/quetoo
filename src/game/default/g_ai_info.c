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

static cvar_t *g_ai_name_prefix;

/**
 * @brief The static roster of bot definitions.
 * Each entry defines a unique bot with its own name, appearance, and personality.
 */
static const g_ai_roster_t g_ai_roster[] = {
  // name          skin                guid                                    skill  aggr   aware
  { "Stroggo",    "qforcer/default",   "ccbb7ca1-03af-448d-b0ab-b9a496472d86", .50f,  .50f,  .50f },
  { "Enforcer",   "guard/default",     "19d4d35d-e19c-43b7-9bbf-cd3ecbbf88d4", .65f,  .60f,  .55f },
  { "Berserker",  "gork/default",      "9fa691aa-dc2d-49b4-a4b4-0c0fad4d755b", .40f,  .85f,  .30f },
  { "Gunner",     "guard/mgss",        "42a7c448-4c4a-434a-9274-69f700b2f8b6", .70f,  .45f,  .70f },
  { "Gladiator",  "dragoon/default",   "c4ad0a99-5251-42fc-bc63-2f705ce6f363", .55f,  .70f,  .45f },
  { "Makron",     "dragoon/baron",     "1ba330d3-6ee3-473c-9d18-bd21d29ec262", .85f,  .55f,  .80f },
  { "Brain",      "gork/ctf",          "82229474-3efc-4872-bd05-a5a997f9a3e6", .75f,  .25f,  .90f },
  { "Widow",      "bunker/default",    "c83474c1-422c-44cd-a93b-918390435187", .60f,  .40f,  .65f },
  { "Tank",       "bunker/hax",        "a66cc25d-e234-4f8d-98b5-7cc4d34ea067", .35f,  .80f,  .35f },
  { "Medic",      "guard/sggrd",       "80baa7ff-2ea6-4d8e-b111-caaaf2147e8b", .45f,  .30f,  .75f },
  { "Parasite",   "dragoon/bastard",   "cbb2fb55-72fc-4242-aa5b-ff9d5f056883", .80f,  .75f,  .60f },
  { "Flyer",      "bunker/fidget",     "cbbe6216-801e-4c53-84bc-f2de6fcc3e1c", .55f,  .65f,  .40f },
};

static const uint32_t g_ai_roster_count = lengthof(g_ai_roster);

/**
 * @brief Round-robin index for assigning bots from the roster.
 */
static uint32_t g_ai_roster_index;

/**
 * @brief Returns true if the given name is already in use by a connected client other than @p cl.
 */
static _Bool G_Ai_NameInUse(const g_client_t *cl, const char *name) {
  G_ForEachClient(other, {
    if (other == cl) {
      continue;
    }
    const char *other_name = InfoString_Get(other->user_info, "name");
    if (other_name && strcmp(other_name, name) == 0) {
      return true;
    }
  });
  return false;
}

/**
 * @brief Create the user info for the specified bot and return its roster entry.
 *
 * @details Assigns the next roster entry in round-robin order. If the base name is
 * already taken by another connected client, appends " 1", " 2", etc. until a
 * unique name is found.
 */
const g_ai_roster_t *G_Ai_GetUserInfo(const g_client_t *cl, char *info) {

  const g_ai_roster_t *entry = &g_ai_roster[g_ai_roster_index];

  g_ai_roster_index = (g_ai_roster_index + 1) % g_ai_roster_count;

  SDL_strlcpy(info, DEFAULT_BOT_INFO, MAX_INFO_STRING_STRING);

  InfoString_Set(info, "skin", entry->skin);
  InfoString_Set(info, "guid", entry->guid);
  InfoString_Set(info, "color", va("%u", RandomRangeu(0, 360)));
  InfoString_Set(info, "hand", va("%u", RandomRangeu(0, 3)));
  InfoString_Set(info, "head", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));
  InfoString_Set(info, "shirt", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));
  InfoString_Set(info, "pants", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));

  char name[MAX_INFO_STRING_VALUE];
  SDL_snprintf(name, sizeof(name), "%s%s", g_ai_name_prefix->string, entry->name);
  for (uint32_t suffix = 1; G_Ai_NameInUse(cl, name); suffix++) {
    SDL_snprintf(name, sizeof(name), "%s%s %u", g_ai_name_prefix->string, entry->name, suffix);
  }

  InfoString_Set(info, "name", name);

  return entry;
}

/**
 * @brief Initializes the AI name prefix console variable.
 */
void G_Ai_InitSkins(void) {
  g_ai_name_prefix = gi.AddCvar("g_ai_name_prefix", "^0[^1BOT^0] ^7", 0, NULL);
}

/**
 * @brief Shuts down AI skin and name resources.
 */
void G_Ai_ShutdownSkins(void) {
}
