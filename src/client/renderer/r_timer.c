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

typedef struct {
	_Bool initialized;
	_Bool running;
	_Bool awaiting_results;
	_Bool record_events;

	uint32_t start_time;
	uint32_t num_queries, num_results, num_frames;
	uint32_t depth;
	r_timer_entry_t queries[R_MAX_TIMER_QUERIES];
	r_timer_entry_t *stack;
	GLuint queries_block[R_MAX_TIMER_QUERIES];
	size_t query_block_id;
	GLuint last_query;

	file_t *f;
} r_timer_queries_t;

static r_timer_queries_t r_timer_queries;

/**
 * @brief Get a free query ID. Allocates a new block of IDs if we run out.
 */
static GLuint R_GetQueryId(void) {

	if (r_timer_queries.query_block_id == lengthof(r_timer_queries.queries)) {
		glad_glGenQueries(lengthof(r_timer_queries.queries), r_timer_queries.queries_block);
		r_timer_queries.query_block_id = 0;

		Com_Verbose("Grew timer query ID block\n");
	}

	return r_timer_queries.queries_block[r_timer_queries.query_block_id++];
}

/**
 * @brief Begin a new timer of the specified type, returning an ID to call end with.
 */
uint32_t R_BeginTimer(const char *type) {

	if (!r_timer_queries.running || r_timer_queries.awaiting_results) {
		return UINT32_MAX;
	}

	if (r_timer_queries.num_queries >= R_MAX_TIMER_QUERIES) {
		Com_Warn("Too many queries\n");
		return UINT32_MAX;
	}

	r_timer_entry_t *entry = &r_timer_queries.queries[r_timer_queries.num_queries++];

	g_strlcpy(entry->type, type, sizeof(entry->type));

	if (!entry->start) {
		entry->start = R_GetQueryId();
	}

	glad_glQueryCounter(entry->start, GL_TIMESTAMP);

	entry->depth = r_timer_queries.depth++;

	entry->prev = r_timer_queries.stack;

	entry->num_events = 0;

	r_timer_queries.stack = entry;

	return r_timer_queries.num_queries - 1;
}

/**
 * @brief Finish the specified timer ID.
 */
void R_EndTimer(const uint32_t id) {

	if (!r_timer_queries.running || r_timer_queries.awaiting_results || id == UINT32_MAX) {
		return;
	}

	if (!r_timer_queries.queries[id].end) {
		r_timer_queries.queries[id].end = R_GetQueryId();
	}
	
	glad_glQueryCounter(r_timer_queries.last_query = r_timer_queries.queries[id].end, GL_TIMESTAMP);

	r_timer_queries.depth--;

	if (r_timer_queries.stack) {
		r_timer_queries.stack = r_timer_queries.stack->prev;
	}
}

/**
 * @brief At the start of timer events, capture the timestamp.
 */
static void R_TimerEvent(const char *event) {

	if (!r_timer_queries.record_events || !r_timer_queries.stack || !r_timer_queries.running || r_timer_queries.awaiting_results) {
		return;
	}

	if (r_timer_queries.stack->num_events >= R_MAX_TIMER_QUERIES) {
		Com_Warn("Too many events\n");
		return;
	}

	r_timer_event_t *e = &r_timer_queries.stack->events[r_timer_queries.stack->num_events++];

	e->event_name = event;

	if (!e->start) {
		e->start = R_GetQueryId();
	}

	glad_glQueryCounter(e->start, GL_TIMESTAMP);
}

/**
 * @brief At the end of timer events, capture the timestamp.
 */
static void R_EndTimerEvent(void) {

	if (!r_timer_queries.record_events || !r_timer_queries.stack || !r_timer_queries.running || r_timer_queries.awaiting_results) {
		return;
	}

	r_timer_event_t *e = &r_timer_queries.stack->events[r_timer_queries.stack->num_events - 1];

	if (!e->end) {
		e->end = R_GetQueryId();
	}

	glad_glQueryCounter(e->end, GL_TIMESTAMP);
}

/**
 * @brief Check if the timer subsystem is ready for data capture.
 */
_Bool R_TimersReady(void) {

	return r_timer_queries.running && !r_timer_queries.awaiting_results;
}

/**
 * @brief Fetch the result of the specified query ID.
 */
static uint64_t R_ResolveQuery(const uint32_t id) {
	uint64_t result;
	glad_glGetQueryObjectui64v(id, GL_QUERY_RESULT, &result);
	return result;
}

/**
 * @brief Checks whether the specified query ID is available for results.
 */
static _Bool R_QueryReady(const uint32_t id) {
	int32_t result;
	glad_glGetQueryObjectiv(id, GL_QUERY_RESULT_AVAILABLE, &result);
	return result;
}

/**
 * @brief End-of-frame timer reset & write
 */
void R_ResetTimers(void) {

	if (!r_timer_queries.running) {
		return;
	}

	// flip the waiting switch if we're not waiting yet (just finished frame)
	if (!r_timer_queries.awaiting_results) {
		r_timer_queries.awaiting_results = true;
	}

	if (!R_QueryReady(r_timer_queries.last_query)) {
		return;
	}

	file_t *f;

	const uint64_t offset = R_ResolveQuery(r_timer_queries.queries[0].start);

	if (r_timer_queries.f) {
		f = r_timer_queries.f;
	} else {
		const uint64_t total_ms = (uint64_t) round((R_ResolveQuery(r_timer_queries.last_query) - offset) / 1000000.0);
		const char *filename = va("profiles/profile_%u_%i_%" PRIu64 ".json", r_timer_queries.start_time, r_timer_queries.num_frames, total_ms);
		f = Fs_OpenWrite(filename);

		if (!f) {
			Com_Warn("Couldn't write to %s.\n", Fs_RealPath(filename));
			return;
		}

		Fs_Print(f, "{\"traceEvents\":[\n");
	}

	for (uint32_t i = 0; i < r_timer_queries.num_queries; i++, r_timer_queries.num_results++) {
		const r_timer_entry_t *entry = &r_timer_queries.queries[i];
		const uint64_t start = (uint64_t) round((R_ResolveQuery(entry->start) - offset) / 1000.0);
		const uint64_t end = (uint64_t) round((R_ResolveQuery(entry->end) - offset) / 1000.0);

		if (r_timer_queries.num_results) {
			Fs_Print(f, ",\n");
		}

		Fs_Print(f, "{\"pid\":%u,\"tid\":1,\"ts\":%" PRIu64 ",\"ph\":\"X\",\"cat\":\"gpu\",\"name\":\"%s\",\"dur\":%" PRIu64 "}", r_timer_queries.num_frames, start, entry->type, (end - start));

		for (uint32_t e = 0; e < entry->num_events; e++) {
			const r_timer_event_t *event = &entry->events[e];

			Fs_Print(f, ",\n");

			const uint64_t event_start = (uint64_t) round((R_ResolveQuery(event->start) - offset) / 1000.0);
			const uint64_t event_end = (uint64_t) round((R_ResolveQuery(event->end) - offset) / 1000.0);
			
			Fs_Print(f, "{\"pid\":%u,\"tid\":1,\"ts\":%" PRIu64 ",\"ph\":\"X\",\"cat\":\"gpu\",\"name\":\"%s\",\"dur\":%" PRIu64 "}", r_timer_queries.num_frames, event_start, event->event_name, (event_end - event_start));
		}
	}

	if (!r_timer_queries.f) {

		r_timer_queries.num_results = 0;

		Fs_Print(f, "]}");

		Fs_Close(f);
	}
	
	r_timer_queries.awaiting_results = false;
	r_timer_queries.num_queries = 0;
	r_timer_queries.num_frames++;

	assert(!r_timer_queries.depth);
}

/**
 * @brief OpenGL no-op pre-call callback.
 */
static void R_Timers_PreCallbackNull(const char *name, GLADapiproc apiproc, int len_args, ...) {
}

/**
 * @brief OpenGL pre-call callback, to capture event data.
 */
static void R_Timers_PreCallback(const char *name, GLADapiproc apiproc, int len_args, ...) {

	R_TimerEvent(name);
}

/**
 * @brief OpenGL post-call callback, to capture event data.
 */
static void R_Timers_PostCallback(void *ret, const char *name, GLADapiproc apiproc, int len_args, ...) {

	R_EndTimerEvent();
}

/**
 * @brief Command callback for starting timers
 */
static void R_StartTimers(void) {

	if (!r_timer_queries.initialized) {

		glGenQueries(lengthof(r_timer_queries.queries), r_timer_queries.queries_block);
		r_timer_queries.query_block_id = 0;

		r_timer_queries.initialized = true;
	}

	r_timer_queries.record_events = false;
	r_timer_queries.num_frames = 0;
	r_timer_queries.start_time = SDL_GetTicks();
	
	Fs_Mkdir("profiles");

	// check flags
	for (int32_t i = 1; i < Cmd_Argc(); i++) {

		if (!stricmp(Cmd_Argv(i), "events")) {
			r_timer_queries.record_events = true;
		} else if (!stricmp(Cmd_Argv(i), "combine")) {
			const char *filename = va("profiles/profile_%u.json", r_timer_queries.start_time);
			file_t *f = Fs_OpenWrite(filename);

			if (!f) {
				Com_Warn("Couldn't write to %s.\n", Fs_RealPath(filename));
				return;
			}
			
			Fs_Print(f, "{\"traceEvents\":[\n");

			Com_Print("Writing timing data to %s.\n", Fs_RealPath(filename));

			r_timer_queries.f = f;
		}
	}

	if (!r_timer_queries.f) {
		Com_Print("Writing timing data per-frame to %s.\n", Fs_RealDir("profiles"));
	}

	r_timer_queries.running = true;

	if (r_timer_queries.record_events) {
		gladSetGLPreCallback(R_Timers_PreCallback);
		gladSetGLPostCallback(R_Timers_PostCallback);
	}

	if (!r_get_error->integer || GLAD_GL_KHR_debug) {
		gladInstallGLDebug();
	}
}

/**
 * @brief Command callback for stopping timers
 */
static void R_StopTimers(void) {

	if (r_timer_queries.f) {
		Fs_Print(r_timer_queries.f, "]}");

		Fs_Close(r_timer_queries.f);
		r_timer_queries.f = NULL;
	}

	Com_Print("Finished writing timing data.\n");

	r_timer_queries.running = false;

	if (r_timer_queries.record_events) {
		gladSetGLPreCallback(R_Timers_PreCallbackNull);
		gladSetGLPostCallback(R_Debug_GladPostCallback);

		if (!r_get_error->integer || GLAD_GL_KHR_debug) {
			gladUninstallGLDebug();
		}
	}
}

/**
 * @brief Initialize the timer subsystem
 */
void R_InitTimers(void) {

	Cmd_Add("r_timers_start", R_StartTimers, CMD_RENDERER, "Start recording timing data.\n usage: r_timers_start [events] [combine]");
	Cmd_Add("r_timers_stop", R_StopTimers, CMD_RENDERER, "Stop recording timing data.\n");
}

/**
 * @brief Shut down and reset the state of the timer subsystem
 */
void R_ShutdownTimers(void) {

	if (r_timer_queries.initialized) {
		for (size_t i = 0; i < R_MAX_TIMER_QUERIES; i++) {
			glDeleteQueries(1, &r_timer_queries.queries[i].start);
			glDeleteQueries(1, &r_timer_queries.queries[i].end);

			for (size_t e = 0; e < R_MAX_TIMER_EVENTS; e++) {
				glDeleteQueries(1, &r_timer_queries.queries[i].events[e].start);
				glDeleteQueries(1, &r_timer_queries.queries[i].events[e].end);
			}
		}

		glDeleteQueries((GLsizei) (lengthof(r_timer_queries.queries) - r_timer_queries.query_block_id), r_timer_queries.queries_block);
	}

	memset(&r_timer_queries, 0, sizeof(r_timer_queries));
}