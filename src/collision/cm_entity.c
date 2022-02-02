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

#include "cm_local.h"

/**
 * @brief
 */
GList *Cm_LoadEntities(const char *entity_string) {

	GList *entities = NULL;

	parser_t parser = Parse_Init(entity_string, PARSER_NO_COMMENTS);

	while (true) {

		char token[MAX_BSP_ENTITY_VALUE];

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0("{", token)) {

			cm_entity_t *entity = NULL;

			while (true) {

				cm_entity_t *pair = Mem_TagMalloc(sizeof(*pair), MEM_TAG_COLLISION);

				Parse_Token(&parser, PARSE_DEFAULT, pair->key, sizeof(pair->key));
				Parse_Token(&parser, PARSE_DEFAULT, pair->string, sizeof(pair->string));

				if (strlen(pair->string)) {
					pair->parsed |= ENTITY_STRING;
					pair->nullable_string = pair->string;
				}

				if (Parse_QuickPrimitive(pair->string, PARSER_NO_COMMENTS, PARSE_DEFAULT,
										 PARSE_INT32, &pair->integer, 1) == 1) {
					pair->parsed |= ENTITY_INTEGER;
				}

				const size_t count = Parse_QuickPrimitive(pair->string, PARSER_NO_COMMENTS,
														  PARSE_DEFAULT, PARSE_FLOAT, &pair->vec4, 4);

				switch (count) {
					case 1:
						pair->parsed |= ENTITY_FLOAT;
						break;
					case 2:
						pair->parsed |= ENTITY_VEC2;
						break;
					case 3:
						pair->parsed |= ENTITY_VEC3;
						break;
					case 4:
						pair->parsed |= ENTITY_VEC4;
						break;
				}

				pair->next = entity;
				entity = pair;

				Parse_PeekToken(&parser, PARSE_DEFAULT, token, sizeof(token));

				if (!g_strcmp0("}", token)) {
					break;
				}
			}

			assert(entity);

			entities = g_list_prepend(entities, entity);
		}
	}

	return g_list_reverse(entities);
}

/**
 * @brief
 */
int32_t Cm_EntityNumber(const cm_entity_t *entity) {

	for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++) {
		if (Cm_Bsp()->entities[i] == entity) {
			return i;
		}
	}

	return -1;
}

/**
 * @brief
 */
const cm_entity_t *Cm_EntityValue(const cm_entity_t *entity, const char *key) {
	static cm_entity_t null_entity;

	for (const cm_entity_t *e = entity; e; e = e->next) {
		if (!g_strcmp0(e->key, key)) {
			return e;
		}
	}

	return &null_entity;
}

/**
 * @brief
 */
GPtrArray *Cm_EntityBrushes(const cm_entity_t *entity) {

	GPtrArray *brushes = g_ptr_array_new();

	cm_bsp_brush_t *brush = Cm_Bsp()->brushes;
	for (int32_t i = 0; i < Cm_Bsp()->num_brushes; i++, brush++) {

		if (brush->entity == entity) {
			g_ptr_array_add(brushes, brush);
		}
	}

	return brushes;
}
