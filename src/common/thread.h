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

#include <SDL_thread.h>

#include "mem.h"

#define MAX_THREADS 128

thread_t *Thread_Create_(const char *name, ThreadRunFunc run, void *data, thread_options_t options);
#define Thread_Create(function, data, options) Thread_Create_(#function, function, data, options)
void Thread_Wait(thread_t *t);
int32_t Thread_Count(void);
void Thread_Init(ssize_t num_threads);
void Thread_Shutdown(void);

extern SDL_threadID thread_main;
extern _Thread_local SDL_threadID thread_id;
