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

#include "entity.h"

/**
 * @brief
 */
void SetValueForKey(entity_t *ent, const char *key, const char *value) {

	for (entity_key_value_t *e = ent->values; e; e = e->next) {
		if (!g_strcmp0(e->key, key)) {
			g_strlcpy(e->value, value, sizeof(e->value));
			return;
		}
	}

	entity_key_value_t *e = Mem_TagMalloc(sizeof(*e), MEM_TAG_EPAIR);
	e->next = ent->values;
	ent->values = e;

	g_strlcpy(e->key, key, sizeof(e->key));
	g_strlcpy(e->value, value, sizeof(e->value));
}

/**
 * @brief
 */
const char *ValueForKey(const entity_t *ent, const char *key, const char *def) {

	for (const entity_key_value_t *e = ent->values; e; e = e->next) {
		if (!g_strcmp0(e->key, key)) {
			return e->value;
		}
	}

	return def;
}

/**
 * @brief
 */
vec3_t VectorForKey(const entity_t *ent, const char *key, const vec3_t def) {

	const char *value = ValueForKey(ent, key, NULL);
	if (value) {
		vec3_t out;
		if (sscanf(value, "%f %f %f", &out.x, &out.y, &out.z) == 3) {
			return out;
		}
	}

	return def;
}
