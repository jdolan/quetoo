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

#include "g_local.h"

/**
 * @brief
 */
void G_Sound(const g_play_sound_t *play) {
	vec3_t from;

	assert(play->index > -1);
	assert(play->index < MAX_SOUNDS);

	int32_t flags = 0;

	if (play->entity) {
		flags |= SOUND_ENTITY;
		from = Box3_Center(play->entity->abs_bounds);
	}

	if (play->origin) {
		flags |= SOUND_ORIGIN;
		from = *play->origin;
	}

	if (play->atten) {
		flags |= SOUND_ATTEN;
	}

	if (play->pitch) {
		flags |= SOUND_PITCH;
	}

	gi.WriteByte(SV_CMD_SOUND);
	gi.WriteByte(flags);
	gi.WriteByte(play->index);

	if (flags & SOUND_ENTITY) {
		gi.WriteShort((uint16_t) (ptrdiff_t) (play->entity - ge.entities));
	}

	if (flags & SOUND_ORIGIN) {
		gi.WritePosition(*play->origin);
	}

	if (flags & SOUND_ATTEN) {
		gi.WriteByte(play->atten);
	}

	if (flags & SOUND_PITCH) {
		gi.WriteChar(play->pitch);
	}

	if (play->atten) {
		gi.Multicast(from, MULTICAST_PHS, NULL);
	} else {
		gi.Multicast(from, MULTICAST_ALL, NULL);
	}
}
