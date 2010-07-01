/**
 * @file scripts.h
 * @brief Header for script parsing functions
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef SCRIPTS_H
#define SCRIPTS_H

#include "shared.h"
#include "m_menus.h"

#include <stdint.h>
#include <assert.h>

#define MAX_VAR 64
#define qboolean unsigned char
#define qfalse false
#define qtrue true
#define _(x) x
#define STRINGIFY(x) #x
#define DOUBLEQUOTE(x) STRINGIFY(x)
#define Vector2Set(v, x, y)     ((v)[0]=(x), (v)[1]=(y))
#define Vector2Copy(v2, v1)     ((v1)[0]=(v2)[0], (v1)[1]=(v2)[2])
#define Vector4Set(v, a, b, c, d)     ((v)[0]=(a), (v)[1]=(b), (v)[2]=(c), (v)[3]=(d))
#define Com_DPrintf(level, ...) Com_Dprintf(__VA_ARGS__)
#define Com_sprintf(dest, size, ...) snprintf(dest, size, __VA_ARGS__)

#define mousePosX (cls.mouse_state.x / r_state.rx)
#define mousePosY (cls.mouse_state.y / r_state.ry)

#ifndef ALIGN_PTR
#define ALIGN_PTR(value,size) (void*)(((uintptr_t)value + (size - 1)) & (~(size - 1)))
#endif

#define MEMBER_SIZEOF(TYPE, MEMBER) sizeof(((TYPE *)0)->MEMBER)

/**
 * @brief Allow to add extra bit into the type
 * @note If valueTypes_t is bigger than 63 the mask must be changed
 * @sa valueTypes_t
 */
#define V_BASETYPEMASK 0x3F

/**
 * @brief possible values for parsing functions
 * @sa vt_names
 * @sa vt_sizes
 */
typedef enum {
	V_NULL,
	V_BOOL,
	V_CHAR,
	V_INT,
	V_INT2,
	V_FLOAT,
	V_POS,
	V_VECTOR,
	V_COLOR,
	V_RGBA,
	V_STRING,
	V_LONGSTRING,			/**< not buffer safe - use this only for menu node data array values! */
	V_ALIGN,
	V_LONGLINES,

	V_NUM_TYPES
} valueTypes_t;

extern const char *const vt_names[];
extern const char *const align_names[];
extern const char *const longlines_names[];

/** @brief We need this here for checking the boundaries from script values */
typedef enum {
	LONGLINES_WRAP,
	LONGLINES_CHOP,
	LONGLINES_PRETTYCHOP,

	LONGLINES_LAST
} longlines_t;

/** used e.g. in our parsers */
typedef struct value_s {
	const char *string;
	const int type;
	const size_t ofs;
	const size_t size;
} value_t;

typedef enum {
	RESULT_ERROR = -1,
	RESULT_WARNING = -2,
	RESULT_OK = 0
} resultStatus_t;

const char* Com_GetLastParseError(void);
const char *Com_EParse(const char **text, const char *errhead, const char *errinfo);
int Com_EParseValue(void *base, const char *token, valueTypes_t type, int ofs, size_t size);
int Com_SetValue(void *base, const void *set, valueTypes_t type, int ofs, size_t size);
void* Com_AlignPtr(void *memory, valueTypes_t type);
const char *Com_ValueToStr(const void *base, const valueTypes_t type, const int ofs);
int Com_ParseValue(void *base, const char *token, valueTypes_t type, int ofs, size_t size, size_t *writedByte);
int Q_vsnprintf(char *str, size_t size, const char *format, va_list ap);
void Q_strncpyz(char *dest, const char *src, size_t destsize);
void Q_strcat(char *dest, const char *src, size_t destsize);
void FS_SkipBlock(const char **text);
const char *Com_MacroExpandString(const char *text);

#define R_Color(color) { if (color == NULL) glColor4ubv(color_white); else glColor4fv(color); }
void R_FontTextSize(const char *fontID, const char *text, int maxWidth, int chop, int *width, int *height, int *lines, qboolean *isTruncated);

#if defined _WIN32
#	define Q_strcasecmp(a, b) _stricmp((a), (b))
#else
#	define Q_strcasecmp(a, b) strcasecmp((a), (b))
#endif

#include "mem.h"

#endif
