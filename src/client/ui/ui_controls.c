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

#include "ui_local.h"

/*
 * Ui_Controls
 */
TwBar *Ui_Controls(void) {
	extern cvar_t *m_sensitivity;
	extern cvar_t *m_invert;
	extern cvar_t *m_interpolate;

	TwBar *bar = TwNewBar("Controls");

	Ui_Bind(bar, "Forward", "+forward", "group=Movement");
	Ui_Bind(bar, "Back", "+back", "group=Movement");
	Ui_Bind(bar, "Move left", "+move_left", "group=Movement");
	Ui_Bind(bar, "Move right", "+move_right", "group=Movement");
	Ui_Bind(bar, "Turn left", "+left", "group=Movement");
	Ui_Bind(bar, "Turn right", "+right", "group=Movement");
	Ui_Bind(bar, "Center view", "center_view", "group=Movement");
	Ui_Bind(bar, "Jump", "+move_up", "group=Movement");
	Ui_Bind(bar, "Crouch", "+move_down", "group=Movement");
	Ui_Bind(bar, "Run / walk", "+speed", "group=Movement");

	Ui_CvarDecimal(bar, "Sensitivity", m_sensitivity, "min=0 max=6 group=Mouse");
	Ui_CvarEnum(bar, "Invert mouse", m_invert, ui.OffOrOn, "group=Mouse");
	Ui_CvarEnum(bar, "Smooth mouse", m_interpolate, ui.OffOrOn, "group=Mouse");

	Ui_Bind(bar, "Shotgun", "use shotgun", "group=Weapons");
	Ui_Bind(bar, "Super shotgun", "use super shotgun", "group=Weapons");
	Ui_Bind(bar, "Machinegun", "use machinegun", "group=Weapons");
	Ui_Bind(bar, "Grenade launcher", "use grenade launcher", "group=Weapons");
	Ui_Bind(bar, "Rocket launcher", "use rocket launcher", "group=Weapons");
	Ui_Bind(bar, "Hyperblaster", "use hyperblaster", "group=Weapons");
	Ui_Bind(bar, "Lightning", "use lightning", "group=Weapons");
	Ui_Bind(bar, "Railgun", "use railgun", "group=Weapons");
	Ui_Bind(bar, "BFG-10K", "use bfg10k", "group=Weapons");

	Ui_Bind(bar, "Next weapon", "weapon_next", "group=Combat");
	Ui_Bind(bar, "Previous weapon", "weapon_prev", "group=Combat");
	Ui_Bind(bar, "Attack", "+attack", "group=Combat");
	Ui_Bind(bar, "Zoom", "+ZOOM", "group=Combat");

	Ui_Bind(bar, "Talk", "message_mode", "group=Communication");
	Ui_Bind(bar, "Talk to team", "message_mode_2", "group=Communication");
	Ui_Bind(bar, "Show scores", "score", "group=Communication");

	TwDefine(
			"Controls size='250 550' alpha=200 iconifiable=false visible=false");

	return bar;
}
