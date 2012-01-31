/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __CL_KEYS_H__
#define __CL_KEYS_H__

#include "cl_types.h"

const char *Cl_KeyNumToString(unsigned short key_num);

#ifdef __CL_LOCAL_H__

typedef struct key_name_s {
	const char *name;
	key_num_t key_num;
} key_name_t;

extern key_name_t key_names[];

void Cl_KeyEvent(unsigned int key, unsigned short unicode, boolean_t down, unsigned time);
char *Cl_EditLine(void);
void Cl_WriteBindings(FILE *f);
void Cl_InitKeys(void);
void Cl_ShutdownKeys(void);
void Cl_ClearTyping(void);

#endif /* __CL_LOCAL_H__ */

#endif /* __CL_KEYS_H__ */
