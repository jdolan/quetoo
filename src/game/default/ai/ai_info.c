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

#include "../g_local.h"

static cvar_t *ai_name_prefix;

/**
 * @brief Type for a single skin string
 */
typedef char ai_skin_t[MAX_QPATH];

/**
 * @brief List of AI skins.
 */
static GArray *ai_skins;

/**
 * @brief Fs_Enumerator for resolving available skins for a given model.
 */
static void Ai_EnumerateSkins(const char *path, void *data) {
	char name[MAX_QPATH];
	char *s = strstr(path, "players/");

	if (s) {
		StripExtension(s + strlen("players/"), name);

		if (g_str_has_suffix(name, "_i")) {
			name[strlen(name) - strlen("_i")] = '\0';

			for (size_t i = 0; i < ai_skins->len; i++) {

				if (g_strcmp0(g_array_index(ai_skins, ai_skin_t, i), name) == 0) {
					return;
				}
			}

			g_array_append_val(ai_skins, name);
		}
	}
}

/**
 * @brief Fs_Enumerator for resolving available models.
 */
static void Ai_EnumerateModels(const char *path, void *data) {
	gi.EnumerateFiles(va("%s/*.tga", path), Ai_EnumerateSkins, NULL);
}

/**
 * @brief
 */
static const char ai_names[][MAX_USER_INFO_VALUE] = {
	"Stroggo",
	"Enforcer",
	"Berserker",
	"Gunner",
	"Gladiator",
	"Makron",
	"Brain"
};

/**
 * @brief
 */
static const uint32_t ai_names_count = lengthof(ai_names);

/**
 * @brief Integers for current name index in g_ai_names table
 * and number of times we've wrapped around it.
 */
static uint32_t ai_name_index;
static uint32_t ai_name_suffix;

/**
 * @brief Create the user info for the specified bot entity.
 */
void Ai_GetUserInfo(const g_entity_t *self, char *info) {

	g_strlcpy(info, DEFAULT_BOT_INFO, MAX_USER_INFO_STRING);

	SetUserInfo(info, "skin", g_array_index(ai_skins, ai_skin_t, RandomRangeu(0, ai_skins->len)));
	SetUserInfo(info, "color", va("%u", RandomRangeu(0, 360)));
	SetUserInfo(info, "hand", va("%u", RandomRangeu(0, 3)));
	SetUserInfo(info, "head", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));
	SetUserInfo(info, "shirt", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));
	SetUserInfo(info, "pants", va("%02x%02x%02x", RandomRangeu(0, 256), RandomRangeu(0, 256), RandomRangeu(0, 256)));

	if (ai_name_suffix == 0) {
		SetUserInfo(info, "name", va("%s%s", ai_name_prefix->string, ai_names[ai_name_index]));
	} else {
		SetUserInfo(info, "name", va("%s%s %i", ai_name_prefix->string, ai_names[ai_name_index], ai_name_suffix + 1));
	}

	ai_name_index++;

	if (ai_name_index == ai_names_count) {
		ai_name_index = 0;
		ai_name_suffix++;
	}
}

/**
 * @brief
 */
void Ai_InitSkins(void) {

	ai_name_prefix = gi.AddCvar("ai_name_prefix", "^0[^1BOT^0] ^7", 0, NULL);

	ai_skins = g_array_new(false, false, sizeof(ai_skin_t));

	gi.EnumerateFiles("players/*", Ai_EnumerateModels, NULL);
}

/**
 * @brief
 */
void Ai_ShutdownSkins(void) {
	g_array_free(ai_skins, true);
	ai_skins = NULL;
}
