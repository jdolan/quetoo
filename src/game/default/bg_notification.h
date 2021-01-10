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
 * @brief Notification types.
 */
typedef enum {
	NOTIFICATION_OBITUARY,
	NOTIFICATION_OBITUARY_SELF,
	NOTIFICATION_PLAYER_EVENT,
	NOTIFICATION_GAME_EVENT
} bg_NOTIFICATION_t;

/**
 * @brief Notifications.
 */
typedef struct {
	/**
	 * @brief The notification type.
	 */
	bg_NOTIFICATION_t type;

	/**
	 * @brief The notification string.
	 */
	char string[MAX_STRING_CHARS];

	/**
	 * @brief The subject entity number (typically a client).
	 */
	uint16_t subject;

	/**
	 * @brief The object entity number (typically a client).
	 */
	uint16_t object;

	/**
	 * @brief The notification pic.
	 */
	uint16_t pic;

	/**
	 * @brief The type specific tag or data for this notification.
	 * @details For obituaries, this is the means of death.
	 */
	int32_t tag;

	/**
	 * @brief The expiration timestamp, in milliseconds.
	 * @details This is a client-side attribtue, not used on the server.
	 */
	uint32_t expiration;
} bg_notification_t;

const char *Bg_GetModString(const uint32_t mod, const _Bool friendly_fire);
const char *Bg_GetModIconString(const uint32_t mod, const _Bool friendly_fire);
