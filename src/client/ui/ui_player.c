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

#include "ui_local.h"

/**
 * @brief Fs_Enumerate function for resolving available skins for a give model.
 */
static void Ui_PlayerSkins_enumerateSkins(const char *path, void *data) {

	char *s = strstr(path, "players/");
	if (s) {
		char name[MAX_QPATH];
		StripExtension(s + strlen("players/"), name);

		if (g_str_has_suffix(name, "_i")) {

			name[strlen(name) - strlen("_i")] = '\0';

			GList **list = (GList **) data;
			*list = g_list_append(*list, g_strdup(name));
		}
	}
}

/**
 * @brief Fs_Enumerate function for resolving available models.
 */
static void Ui_PlayerSkins_enumerateModels(const char *path, void *data) {
	Fs_Enumerate(va("%s/*.tga", path), Ui_PlayerSkins_enumerateSkins, data);
}

/**
 * @brief GCompareFunc for Ui_PlayerSkins.
 */
static int32_t Ui_PlayerSkins_sort(const void *a, const void *b) {
	return g_strcmp0((const char *) a, (const char *) b);
}

/**
 * @return A NULL-terminated array of TwEnumVal for all available player skins.
 */
static TwEnumVal *Ui_PlayerSkins(void) {

	GList *list = NULL;

	Fs_Enumerate("players/*", Ui_PlayerSkins_enumerateModels, &list);

	list = g_list_sort(list, Ui_PlayerSkins_sort);
	const size_t count = g_list_length(list);

	TwEnumVal *skins = Mem_TagMalloc((count + 1) * sizeof(TwEnumVal), MEM_TAG_UI);

	for (size_t i = 0; i < count; i++) {

		char *label = Mem_CopyString(g_list_nth_data(list, i));
		Mem_Link(skins, label);

		skins[i].Label = label;
		skins[i].Value = i;
	}

	g_list_free_full(list, g_free);

	return skins;
}

/**
 * @brief
 */
TwBar *Ui_Player(void) {

	const TwEnumVal *Skins = Ui_PlayerSkins();

	size_t num_Skins = 0;
	for (const TwEnumVal *skin = Skins; skin->Label; skin++, num_Skins++);

	static const TwEnumVal Colors[] = {
		{ 0, "Default" },
		{ 1, "Red" },
		{ 2, "Green" },
		{ 3, "Blue" },
		{ 4, "Yellow" },
		{ 5, "Orange" },
		{ 6, "White" },
		{ 7, "Pink" },
		{ 8, "Purple" },
		{ 9, NULL }
	};

	TwType skins = TwDefineEnum("Skins", Skins, num_Skins);
	TwType colors = TwDefineEnum("Colors", Colors, lengthof(Colors) - 1);

	TwBar *bar = TwNewBar("Player");

	Ui_CvarText(bar, "Name", name, NULL);
	Ui_CvarSelect(bar, "Skin", skin, skins, Skins, NULL);
	Ui_CvarSelect(bar, "Effects color", color, colors, Colors, NULL);

	TwDefine("Player size='350 110' alpha=200 iconifiable=false valueswidth=175 visible=false");

	return bar;
}
