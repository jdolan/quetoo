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

#include "parse.h"

/**
 * @brief
 */
GList *Cm_LoadEntities(const char *entity_string) {

	GList *entities = NULL;

	parser_t parser;
	Parse_Init(&parser, entity_string, PARSER_NO_COMMENTS);

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
				}

				if (sscanf(pair->string, "%d", &pair->integer) == 1) {
					pair->parsed |= ENTITY_INTEGER;
				}

				const int32_t count = sscanf(pair->string, "%f %f %f %f",
											 &pair->vec4.x,
											 &pair->vec4.y,
											 &pair->vec4.z,
											 &pair->vec4.w);
				switch (count) {
					case 1:
						pair->parsed |= ENTITY_FLOAT;
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
const cm_entity_t *Cm_EntityValue(const cm_entity_t *entity, const char *key) {
	static cm_entity_t null_entity;

	for (const cm_entity_t *e = entity; e; e = e->next) {
		if (!g_strcmp0(e->key, key)) {
			return e;
		}
	}

	return &null_entity;
}
