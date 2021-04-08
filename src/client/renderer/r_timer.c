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

#include "r_local.h"

r_timer_queries_t r_timer_queries;

void R_BeginTimer(const char *type) {

	if (!r_timer_queries.initialized) {
		return;
	}

	r_timer_entry_t *entry = &r_timer_queries.queries[r_timer_queries.num_queries++];
	g_strlcpy(entry->type, type, sizeof(entry->type));

	glBeginQuery(GL_TIME_ELAPSED, entry->id);
}

void R_EndTimer(void) {

	if (!r_timer_queries.initialized) {
		return;
	}

	glEndQuery(GL_TIME_ELAPSED);
}

uint64_t R_ResolveQuery(const uint32_t id) {
	uint64_t result;
	glGetQueryObjectui64v(r_timer_queries.queries[id].id, GL_QUERY_RESULT, &result);
	return result;
}

void R_ResetTimers(void) {

	if (!r_timer_queries.initialized) {
		return;
	}

	r_timer_queries.num_queries = 0;
}

void R_InitTimers(void) {

	if (!r_timers->integer) {
		r_timer_queries.initialized = false;
		return;
	}

	for (size_t i = 0; i < R_MAX_TIMER_QUERIES; i++) {
		glGenQueries(1, &r_timer_queries.queries[i].id);
	}
	r_timer_queries.initialized = true;
}

void R_ShutdownTimers(void) {

	if (!r_timer_queries.initialized) {
		return;
	}
	
	for (size_t i = 0; i < R_MAX_TIMER_QUERIES; i++) {
		glDeleteQueries(1, &r_timer_queries.queries[i].id);
	}
	memset(&r_timer_queries, 0, sizeof(r_timer_queries));
}