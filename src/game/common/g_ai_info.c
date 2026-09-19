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

static Cvar *g_ai_name_prefix;

/**
 * @brief The static roster of bot definitions.
 */
static const GameAiRoster g_ai_roster[] = {
  // name          skin                  guid                                    skill  aggr   aware
  { "Enforcer",    "enforcer/default",    "ccbb7ca1-03af-448d-b0ab-b9a496472d86", .50f,  .50f,  .50f },
  { "Guard",       "guard/default",       "19d4d35d-e19c-43b7-9bbf-cd3ecbbf88d4", .65f,  .60f,  .55f },
  { "Gork",        "gork/default",        "9fa691aa-dc2d-49b4-a4b4-0c0fad4d755b", .40f,  .85f,  .30f },
  { "Gunner",      "guard/mgss",          "42a7c448-4c4a-434a-9274-69f700b2f8b6", .70f,  .45f,  .70f },
  { "Dragoon",     "dragoon/default",     "c4ad0a99-5251-42fc-bc63-2f705ce6f363", .55f,  .70f,  .45f },
  { "Makron",      "dragoon/baron",       "1ba330d3-6ee3-473c-9d18-bd21d29ec262", .85f,  .55f,  .80f },
  { "Brain",       "gork/ctf",            "82229474-3efc-4872-bd05-a5a997f9a3e6", .75f,  .25f,  .90f },
  { "Bunker",      "bunker/default",      "c83474c1-422c-44cd-a93b-918390435187", .60f,  .40f,  .65f },
  { "Tank",        "bunker/hax",          "a66cc25d-e234-4f8d-98b5-7cc4d34ea067", .35f,  .80f,  .35f },
  { "Medic",       "guard/sggrd",         "80baa7ff-2ea6-4d8e-b111-caaaf2147e8b", .45f,  .30f,  .75f },
  { "Parasite",    "dragoon/bastard",     "cbb2fb55-72fc-4242-aa5b-ff9d5f056883", .80f,  .75f,  .60f },
  { "Flyer",       "bunker/fidget",       "cbbe6216-801e-4c53-84bc-f2de6fcc3e1c", .55f,  .65f,  .40f },
  { "Bldseekr",    "bloodseeker/default", "928314d3-7eff-463a-ba7b-a0e5348034df", .65f,  .80f,  .70f },
  { "Caustic",     "caustic/default",     "44e9f329-78b8-48cf-82b2-6b2deab9a7e2", .60f,  .40f,  .80f },
  { "Creech",      "creech/default",      "5c314256-99bc-4acf-a1aa-cca6c8628504", .40f,  .75f,  .35f },
  { "Cruentus",    "cruentus/default",    "9c28ccc7-485c-4f64-9114-e1b3fbf5a927", .50f,  .85f,  .30f },
  { "Gammy",       "gammy/default",       "62b66b49-3413-4551-8a78-31b829d3d9c0", .35f,  .55f,  .50f },
  { "Gaunt",       "gaunt/default",       "c302fa4a-5e87-44ba-ad2f-bb5c7c4c3d56", .75f,  .30f,  .85f },
  { "Gladiator",   "gladiator/default",   "b905851b-c639-4535-aaa6-cb6360ac8633", .65f,  .65f,  .55f },
  { "Magdalena",   "magdalena/default",   "2feaddd0-47f6-4457-9311-745774913ea4", .70f,  .45f,  .75f },
  { "Mantis",      "mantis/default",      "c1100255-64db-4e34-aec7-bffc0c772a19", .70f,  .70f,  .50f },
  { "Merc",        "merc/default",        "9a25c51a-7b1b-496b-8316-e506b6122ec3", .75f,  .55f,  .70f },
  { "Nitro",       "nitro/default",       "f8fdb0a8-b25c-411a-b2c2-b59b4933eea2", .45f,  .90f,  .25f },
  { "Sarge",       "sarge/default",       "f1867a5c-75cd-4825-be08-2f036d2325d5", .50f,  .70f,  .45f },
  { "Reaper",      "violator/default",    "17a3bcbb-e622-4736-a0c0-6c6ce64a9ee4", .80f,  .60f,  .65f },
};

static const uint32_t g_ai_roster_count = lengthof(g_ai_roster);

/**
 * @brief Shuffled order in which roster entries are handed out. Reshuffled
 * each time it is exhausted so that every entry is used exactly once per
 * cycle, in a random order, before any entry repeats.
 */
static uint32_t g_ai_roster_order[lengthof(g_ai_roster)];

/**
 * @brief Index of the next entry to hand out from g_ai_roster_order.
 */
static uint32_t g_ai_roster_index;

/**
 * @brief Reshuffles g_ai_roster_order in place using a Fisher-Yates shuffle.
 */
static void G_Ai_ShuffleRoster(void) {

  for (uint32_t i = 0; i < g_ai_roster_count; i++) {
    g_ai_roster_order[i] = i;
  }

  for (uint32_t i = g_ai_roster_count - 1; i > 0; i--) {
    const uint32_t j = RandomRangeu(0, i + 1);
    const uint32_t tmp = g_ai_roster_order[i];
    g_ai_roster_order[i] = g_ai_roster_order[j];
    g_ai_roster_order[j] = tmp;
  }

  g_ai_roster_index = 0;
}

/**
 * @brief Returns true if the given name is already in use by a connected client other than @p cl.
 */
static _Bool G_Ai_NameInUse(const GameClient *cl, const char *name) {

  G_ForEachClient(other, {
    if (other == cl) {
      continue;
    }
    const char *otherName = InfoString_Get(other->userInfo, "name");
    if (otherName && q_strcmp(otherName, name) == 0) {
      return true;
    }
  });

  return false;
}

/**
 * @brief Create the user info for the specified bot and return its roster entry.
 *
 * @details Assigns the next roster entry from a shuffled order, so that entries
 * are handed out in a random sequence without repeating until every entry has
 * been used, at which point the order is reshuffled. If the base name is
 * already taken by another connected client, appends " 1", " 2", etc. until a
 * unique name is found.
 */
const GameAiRoster *G_Ai_GetUserInfo(const GameClient *cl, char *info) {

  if (g_ai_roster_index == g_ai_roster_count) {
    G_Ai_ShuffleRoster();
  }

  const GameAiRoster *entry = &g_ai_roster[g_ai_roster_order[g_ai_roster_index]];

  g_ai_roster_index++;

  q_strlcpy(info, DEFAULT_BOT_INFO, MAX_INFO_STRING_STRING);

  InfoString_Set(info, "skin", entry->skin);
  InfoString_Set(info, "guid", entry->guid);
  InfoString_Set(info, "color", va("%u", RandomRangeu(0, 360)));
  InfoString_Set(info, "hand", va("%u", RandomRangeu(0, 3)));
  InfoString_Set(info, "head", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));
  InfoString_Set(info, "shirt", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));
  InfoString_Set(info, "pants", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));

  char name[MAX_INFO_STRING_VALUE];
  q_snprintf(name, sizeof(name), "%s%s", g_ai_name_prefix->string, entry->name);
  for (uint32_t suffix = 1; G_Ai_NameInUse(cl, name); suffix++) {
    q_snprintf(name, sizeof(name), "%s%s %u", g_ai_name_prefix->string, entry->name, suffix);
  }

  InfoString_Set(info, "name", name);

  return entry;
}

/**
 * @brief Initializes the AI name prefix console variable.
 */
void G_Ai_InitSkins(void) {
  g_ai_name_prefix = gi.AddCvar("g_ai_name_prefix", "^0[^1BOT^0] ^7", 0, NULL);
  G_Ai_ShuffleRoster();
}

/**
 * @brief Shuts down AI skin and name resources.
 */
void G_Ai_ShutdownSkins(void) {
}
