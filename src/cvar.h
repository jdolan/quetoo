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

#ifndef __CVAR_H__
#define __CVAR_H__

#include "cmd.h"
#include "filesystem.h"

extern _Bool cvar_user_info_modified;

typedef void (*CvarEnumerateFunc)(cvar_t *var, void *data);

cvar_t *Cvar_Add(const char *name, const char *value, uint32_t flags, const char *description);
cvar_t *Cvar_Get(const char *name);
cvar_t *Cvar_Set(const char *name, const char *value);
cvar_t *Cvar_ForceSet(const char *name, const char *value);
cvar_t *Cvar_FullSet(const char *name, const char *value, uint32_t flags);
cvar_t *Cvar_SetValue(const char *name, vec_t value);
cvar_t *Cvar_Toggle(const char *name);
vec_t Cvar_GetValue(const char *name);
const char *Cvar_GetString(const char *name);
void Cvar_Enumerate(CvarEnumerateFunc func, void *data);
void Cvar_CompleteVar(const char *pattern, GList **matches);
void Cvar_ResetLocal(void);
_Bool Cvar_PendingLatched(void);
void Cvar_UpdateLatched(void);
_Bool Cvar_Pending(uint32_t flags);
void Cvar_ClearAll(uint32_t flags);
_Bool Cvar_Command(void);
char *Cvar_UserInfo(void);
char *Cvar_ServerInfo(void);
void Cvar_WriteAll(file_t *f);
void Cvar_Init(void);
void Cvar_Shutdown(void);

#endif /* __CVAR_H__ */
