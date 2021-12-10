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
#define R_MAX_TIMER_EVENTS 2048

typedef struct {
	const char *event_name;
	GLuint start, end;
} r_timer_event_t;

typedef struct r_timer_entry_s {
	GLuint start, end;
	char type[MAX_OS_PATH];
	uint32_t depth;
	uint32_t num_events;
	r_timer_event_t events[R_MAX_TIMER_EVENTS];
	struct r_timer_entry_s *prev;
} r_timer_entry_t;

void R_InitTimers(void);
void R_ShutdownTimers(void);
void R_ResetTimers(void);
_Bool R_TimersReady(void);

#define R_TIMER_WRAP(type, ...) { \
	uint32_t timer_id = UINT32_MAX; \
	if (R_TimersReady()) { \
		timer_id = R_BeginTimer(type); \
	} \
	__VA_ARGS__ \
	if (timer_id != UINT32_MAX) { \
		R_EndTimer(timer_id); \
	} \
}

#ifdef __R_LOCAL_H__
uint32_t R_BeginTimer(const char *type);
void R_EndTimer(const uint32_t id);
#endif /* __R_LOCAL_H__ */
