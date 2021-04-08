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

#pragma once

#include "r_types.h"

#define R_MAX_TIMER_QUERIES 1024

typedef struct {
	GLuint id;
	char type[MAX_QPATH];
} r_timer_entry_t;

typedef struct {
	_Bool initialized;
	uint32_t num_queries;
	r_timer_entry_t queries[R_MAX_TIMER_QUERIES];

	GLuint query_ids[R_MAX_TIMER_QUERIES];
} r_timer_queries_t;

extern r_timer_queries_t r_timer_queries;

void R_InitTimers(void);
void R_ShutdownTimers(void);
void R_ResetTimers(void);
uint64_t R_ResolveQuery(const uint32_t id);

#define R_TIMER_WRAP(type, ...) \
	if (r_timers->integer) { R_BeginTimer(type); } \
	__VA_ARGS__ \
	if (r_timers->integer) { R_EndTimer(); }

#ifdef __R_LOCAL_H__
void R_BeginTimer(const char *type);
void R_EndTimer(void);
#endif /* __R_LOCAL_H__ */
