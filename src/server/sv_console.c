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

#include "sv_local.h"

static console_t sv_console;

/*
 * @brief
 */
void Sv_DrawConsole(void) {

	Sv_DrawConsole_Curses();
}

/*
 * @brief
 */
void Sv_InitConsole(void) {

#if defined(_WIN32)
	if (dedicated->value) {
		if (AllocConsole()) {
			freopen("CONIN$", "r", stdin);
			freopen("CONOUT$", "w", stdout);
			freopen("CONERR$", "w", stderr);
		} else {
			Com_Warn("Failed to allocate console: %u\n", (uint32_t) GetLastError());
		}
	}
#endif

	memset(&sv_console, 0, sizeof(sv_console));

#if HAVE_CURSES
	Sv_InitConsole_Curses();
#else
	Sv_InitConsole_Stdout();
#endif
}

/*
 * @brief
 */
void Sv_ShutdownConsole(void) {
#if HAVE_CURSES
	Curses_Shutdown();
#endif

#if defined(_WIN32)
	if (dedicated->value) {
		FreeConsole();
	}
#endif
}
