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

cvar_t *sv_voice;
cvar_t *sv_voice_rate;

/**
 * @brief Initializes the server side of voice chat.
 */
void Sv_InitVoice(void) {

  sv_voice = Cvar_Add("sv_voice", "1", CVAR_SERVER_INFO, "Enables voice chat relaying on this server");
  sv_voice_rate = Cvar_Add("sv_voice_rate", "4000", 0, "The per-client voice chat budget, in bytes per second");
}
