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
				Parse_Token(&parser, PARSE_DEFAULT, pair->value, sizeof(pair->value));

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
const char *Cm_EntityValue(const cm_entity_t *entity, const char *key) {

	for (const cm_entity_t *e = entity; e; e = e->next) {
		if (!g_strcmp0(e->key, key)) {
			return e->value;
		}
	}

	return NULL;
}

/**
 * @brief
 */
size_t Cm_EntityVector(const cm_entity_t *entity, const char *key, float *out, size_t count) {

	parser_t parser;
	Parse_Init(&parser, Cm_EntityValue(entity, key), PARSER_DEFAULT);

	return Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, out, count);
}
