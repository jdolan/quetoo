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

typedef struct {
	const char *map;
} ui_create_server_t;

static ui_create_server_t ui_create_server;

/**
 * @brief Fs_Enumerate function for resolving available maps.
 */
static void Ui_Maps_enumerate(const char *path, void *data) {
	char name[MAX_QPATH];

	StripExtension(Basename(path), name);

	GList **list = (GList **) data;

	for (GList *map = *list; map; map = map->next) {
		if (g_strcmp0(map->data, name) == 0) {
			return;
		}
	}

	*list = g_list_append(*list, g_strdup(name));
}

/**
 * @brief GCompareFunc for Ui_Maps.
 */
static int32_t Ui_Maps_sort(const void *a, const void *b) {
	return g_strcmp0((const char *) a, (const char *) b);
}

/**
 * @return A NULL-terminated array of TwEnumVal for available maps.
 */
static TwEnumVal *Ui_Maps(void) {

	GList *list = NULL;

	Fs_Enumerate("maps/*.bsp", Ui_Maps_enumerate, &list);

	list = g_list_sort(list, Ui_Maps_sort);
	const size_t count = g_list_length(list);

	TwEnumVal *maps = Mem_TagMalloc((count + 1) * sizeof(TwEnumVal), MEM_TAG_UI);

	for (size_t i = 0; i < count; i++) {
		char *label = Mem_CopyString(g_list_nth_data(list, i));

		maps[i].Label = Mem_Link(label, maps);
		maps[i].Value = i;
	}

	g_list_free_full(list, g_free);

	return maps;
}

/**
 * @brief Callback for map selection.
 */
static void TW_CALL Ui_CreateServer_SetMap(const void *value, void *data) {

	for (const TwEnumVal *val = (const TwEnumVal *) data; val->Label; val++) {
		if (val->Value == *(int32_t *) value) {
			ui_create_server.map = val->Label;
			return;
		}
	}
}

/**
 * @brief Callback for map selection.
 */
static void TW_CALL Ui_CreateServer_GetMap(void *value, void *data) {

	int32_t v = 0;

	for (const TwEnumVal *val = (const TwEnumVal *) data; val->Label; val++) {
		if (g_ascii_strcasecmp(val->Label, ui_create_server.map) == 0) {
			v = val->Value;
			break;
		}
	}

	*(int32_t *) value = v;
}

/**
 * @brief Call back for server creation.
 */
static void TW_CALL Ui_CreateServer_Create(void *data) {

	if (ui_create_server.map) {
		char cmd[MAX_STRING_CHARS];

		g_snprintf(cmd, sizeof(cmd), "map %s;", ui_create_server.map);
		Cbuf_AddText(cmd);
	}
}

/**
 * @brief
 */
TwBar *Ui_CreateServer(void) {

	const TwEnumVal *maps = Ui_Maps();
	TwType Maps = TwDefineEnum("Maps", maps, Ui_EnumLength(maps));

	TwBar *bar = TwNewBar("Host Game");

	TwAddVarCB(bar, "Map", Maps, Ui_CreateServer_SetMap, Ui_CreateServer_GetMap, (void *) maps, NULL);

	TwAddSeparator(bar, NULL, NULL);
	TwAddButton(bar, "Host Game", Ui_CreateServer_Create, NULL, NULL);

	TwDefine("'Host Game' size='450 400' alpha=200 iconifiable=false visible=false");

	return bar;
}
