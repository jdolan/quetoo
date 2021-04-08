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

uint32_t R_BeginTimer(const char *type) {

	if (!r_timer_queries.initialized) {
		return -1;
	}

	r_timer_entry_t *entry = &r_timer_queries.queries[r_timer_queries.num_queries++];

	g_strlcpy(entry->type, type, sizeof(entry->type));

	glQueryCounter(entry->start, GL_TIMESTAMP);

	entry->depth = r_timer_queries.depth++;

	entry->prev = r_timer_queries.stack;

	r_timer_queries.stack = entry;

	return r_timer_queries.num_queries - 1;
}

void R_EndTimer(const uint32_t id) {

	if (!r_timer_queries.initialized) {
		return;
	}
	
	glQueryCounter(r_timer_queries.queries[id].end, GL_TIMESTAMP);

	r_timer_queries.depth--;

	if (r_timer_queries.stack) {
		r_timer_queries.stack = r_timer_queries.stack->prev;
	}
}

_Bool R_TimersReady(void) {
	return r_timers->integer && !r_timer_queries.awaiting_results;
}

static uint64_t R_ResolveQuery(const uint32_t id) {
	uint64_t result;
	glGetQueryObjectui64v(id, GL_QUERY_RESULT, &result);
	return result;
}

static _Bool R_QueryReady(const uint32_t id) {
	int32_t result;
	glGetQueryObjectiv(id, GL_QUERY_RESULT_AVAILABLE, &result);
	return result;
}

static void R_WriteTimers(const char *key, const r_timer_result_t *value, file_t *f) {
	Fs_Print(f, "%s,%" PRIu64 ",%" PRIu64 ", %u\n", value->type, value->min, value->max, value->depth);
}

static const char *R_GetTimerType(const r_timer_entry_t *timer) {
	static char temp[MAX_OS_PATH * 6];

	g_strlcpy(temp, timer->type, sizeof(temp));

	for (const r_timer_entry_t *p = timer->prev; p; p = p->prev) {
		const size_t ptype_len = strlen(p->type);

		memmove(temp + ptype_len + 2, temp, strlen(temp) + 1);
		memcpy(temp, p->type, ptype_len);
		temp[ptype_len] = ':';
		temp[ptype_len + 1] = ':';
	}

	return temp;
}

void R_ResetTimers(void) {

	if (!r_timer_queries.initialized) {
		return;
	}

	// flip the waiting switch if we're not waiting yet (just finished frame)
	if (!r_timer_queries.awaiting_results) {
		r_timer_queries.awaiting_results = true;
	}

	_Bool results_ready = true;

	for (uint32_t i = 0; i < r_timer_queries.num_queries; i++) {
		if (!R_QueryReady(r_timer_queries.queries[i].start) ||
			!R_QueryReady(r_timer_queries.queries[i].end)) {
			results_ready = false;
			break;
		}
	}

	if (!results_ready) {
		return;
	}

	for (uint32_t i = 0; i < r_timer_queries.num_queries; i++) {
		const r_timer_entry_t *entry = &r_timer_queries.queries[i];
		const uint64_t start = R_ResolveQuery(entry->start);
		const uint64_t end = R_ResolveQuery(entry->end);
		const uint64_t duration = end - start;
		const char *type = R_GetTimerType(entry);

		r_timer_result_t *result = g_hash_table_lookup(r_timer_queries.results, type);

		if (!result) {
			result = Mem_Malloc(sizeof(r_timer_result_t));
			result->min = result->max = duration;
			g_strlcpy(result->type, type, sizeof(result->type));
			result->depth = entry->depth;
			g_hash_table_insert(r_timer_queries.results, (gpointer) result->type, result);
		} else {
			result->min = Minui64(result->min, duration);
			result->max = Maxui64(result->max, duration);
		}
	}
	
	r_timer_queries.awaiting_results = false;
	r_timer_queries.num_queries = 0;
	r_timer_queries.num_results++;

	assert(!r_timer_queries.depth);

	if (r_timers->modified && !r_timers->integer) {

		file_t *f = Fs_OpenWrite("timers.csv");

		if (!f) {
			Com_Warn("Couldn't write to timers.csv\n");
		} else {
			Fs_Print(f, "Type,Min,Max,Depth\n");

			g_hash_table_foreach(r_timer_queries.results, (GHFunc) R_WriteTimers, f);

			Fs_Close(f);

			Com_Print("Timers written to timers.csv\n");
		}

		r_timers->modified = false;
	}
}

void R_InitTimers(void) {

	if (!r_timers->integer) {
		r_timer_queries.initialized = false;
		return;
	}

	for (size_t i = 0; i < R_MAX_TIMER_QUERIES; i++) {
		glGenQueries(1, &r_timer_queries.queries[i].start);
		glGenQueries(1, &r_timer_queries.queries[i].end);
	}

	r_timer_queries.results = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Mem_Free);

	r_timer_queries.initialized = true;
}

void R_ShutdownTimers(void) {

	if (!r_timer_queries.initialized) {
		return;
	}
	
	for (size_t i = 0; i < R_MAX_TIMER_QUERIES; i++) {
		glDeleteQueries(1, &r_timer_queries.queries[i].start);
		glDeleteQueries(1, &r_timer_queries.queries[i].end);
	}

	g_hash_table_destroy(r_timer_queries.results);

	memset(&r_timer_queries, 0, sizeof(r_timer_queries));
}