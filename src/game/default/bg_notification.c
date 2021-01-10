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

#include "bg_notification.h"

/**
 * @brief List of MOD strings
 */
static const char *mod_strings[] = {
	"[Unknown]",
	"[Blaster]",
	"[Shotgun]",
	"[Super Shotgun]",
	"[Machinegun]",
	"[Grenade]",
	"[Grenade]",
	"[Rocket]",
	"[Rocket]",
	"[Hyperblaster]",
	"[Hyperblaster]",
	"[Lightning]",
	"[Lightning]",
	"[Railgun]",
	"[BFG10K]",
	"[BFG10K]",
	"[Drown]",
	"[Slime]",
	"[Lava]",
	"[Crushed]",
	"[Telefrag]",
	"[Fall]",
	"[Suicide]",
	"[Explode]",
	"[Hurt]",
	"[Hand Grenade]",
	"[Hand Grenade]",
	"[Hand Grenade]",
	"[Hand Grenade]",
	"[Fireball]",
	"[Hook]",
	"[Act of God]"
};

/**
 * @brief List of MOD icon names
 */
static const char *mod_icons[] = {
	"pics/notifications/i_death",
	"pics/notifications/w_blaster",
	"pics/notifications/w_shotgun",
	"pics/notifications/w_sshotgun",
	"pics/notifications/w_machinegun",
	"pics/notifications/w_glauncher",
	"pics/notifications/w_glauncher",
	"pics/notifications/w_rlauncher",
	"pics/notifications/w_rlauncher",
	"pics/notifications/w_hyperblaster",
	"pics/notifications/w_hyperblaster",
	"pics/notifications/w_lightning",
	"pics/notifications/w_lightning",
	"pics/notifications/w_railgun",
	"pics/notifications/w_bfg",
	"pics/notifications/w_bfg",
	"pics/notifications/i_drown",
	"pics/notifications/i_slime",
	"pics/notifications/i_lava",
	"pics/notifications/i_death",
	"pics/notifications/i_telefrag",
	"pics/notifications/i_death",
	"pics/notifications/i_suicide",
	"pics/notifications/i_death",
	"pics/notifications/i_death",
	"pics/notifications/w_handgrenade",
	"pics/notifications/w_handgrenade",
	"pics/notifications/w_handgrenade",
	"pics/notifications/w_handgrenade",
	"pics/notifications/i_lava",
	"pics/notifications/w_hook",
	"pics/notifications/i_death"
};

/**
 * @brief Returns the MOD's string representation
 */
const char *Bg_GetModString(const uint32_t mod, const _Bool friendly_fire) {
	if (friendly_fire) {
		return "[Teamkill]";
	}

	if (mod >= lengthof(mod_strings)) {
		return "[Invalid MOD]";
	}

	return mod_strings[mod];
}

/**
 * @brief Returns the MOD's icon name representation
 */
const char *Bg_GetModIconString(const uint32_t mod, const _Bool friendly_fire) {
	if (friendly_fire) {
		return "pics/notifications/i_teamkill";
	}

	if (mod >= lengthof(mod_icons)) {
		return "pics/notifications/i_death";
	}

	return mod_icons[mod];
}
