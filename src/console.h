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

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "cmd.h"
#include "cvar.h"

#define CON_NUM_NOTIFY 4
#define CON_SCROLL 5
#define CON_CURSOR_CHAR 0x0b

/*
 * @brief A console string.
 */
typedef struct {

	/*
	 * @brief The null-terminated C string.
	 */
	const char *chars;

	/*
	 * @brief The length of this string in bytes.
	 */
	size_t size;

	/*
	 * @brief The length of this string in printable characters.
	 */
	size_t length;

	/*
	 * @brief The timestamp.
	 */
	uint32_t timestamp;

} console_string_t;

/*
 * @brief The maximum number of characters to buffer.
 */
#define CON_MAX_SIZE (1024 * 1024)

/*
 * @brief The console state.
 */
typedef struct {
	/*
	 * @brief The console strings.
	 */
	GList *strings;

	/*
	 * @brief The length of the console strings in characters.
	 */
	size_t size;

	/*
	 * @brief A lock for coordinating thread access to the console.
	 */
	SDL_mutex *lock;

} console_state_t;

extern console_state_t console_state;

/*
 * @brief The console structure.
 */
typedef struct {

	/*
	 * @brief Console width in characters.
	 */
	uint16_t width;

	/*
	 * @brief Console height in characters.
	 */
	uint16_t height;

	/*
	 * @brief The scroll offset in lines.
	 */
	uint16_t scroll;

	/*
	 * @brief An optional print callback.
	 */
	void (*Print)(console_string_t *str);

} console_t;

void Con_Init(void);
void Con_Shutdown(void);
void Con_AddConsole(const console_t *console);
void Con_RemoveConsole(const console_t *console);
void Con_Print(const char *chars);
size_t Con_Wrap(const char *chars, size_t line_width, const char **lines, size_t max_lines);
size_t Con_Tail(const console_t *console, const char **lines, size_t max_lines);
_Bool Con_CompleteCommand(char *input, uint16_t *pos, uint16_t len);

#endif /* __CONSOLE_H__ */
