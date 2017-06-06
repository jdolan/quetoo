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

#include "shared.h"

/**
 * @brief Notification feed types
 */
typedef enum {
	NOTIFICATION_TYPE_OBITUARY, // Player killed a player (MOD + CID + CID)
	NOTIFICATION_TYPE_OBITUARY_SELF, // Player killed itself (MOD + CID)
	NOTIFICATION_TYPE_OBITUARY_PIC, // Player killed a player (PIC + CID + CID)
	NOTIFICATION_TYPE_FINISH, // Race times, capture times (PIC + CID + MILLIS)
} bg_notification_type_t;

/**
 * @brief Notification feed
 */
typedef struct bg_notification_item_s {
	bg_notification_type_t type;

	char string_1[MAX_STRING_CHARS]; // STRING
	char string_2[MAX_STRING_CHARS]; // STRING

	uint16_t client_id_1; // CID
	uint16_t client_id_2; // CID

	char pic[MAX_QPATH]; // PIC

	uint32_t mod; // MOD. FIXME: make this a g_mod_t

	uint32_t millis; // MILLIS

	uint32_t when; // The time this feed item appeared at (used in cgame)
} bg_notification_item_t;

const char *Bg_GetModString(const uint32_t mod, const _Bool friendly_fire);
const char *Bg_GetModIcon(const uint32_t mod, const _Bool friendly_fire);
