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
TwBar *Ui_Controls(void){
	extern cvar_t *m_sensitivity;
	extern cvar_t *m_invert;
	extern cvar_t *m_interpolate;

	TwBar *bar = TwNewBar("Controls");

	TwAddSeparator(bar, "Movement", NULL);

	Ui_Bind(bar, "Forward", "+forward");
	Ui_Bind(bar, "Back", "+back");
	Ui_Bind(bar, "Move left", "+move_left");
	Ui_Bind(bar, "Move right", "+move_right");
	Ui_Bind(bar, "Turn left", "+left");
	Ui_Bind(bar, "Turn right", "+right");
	Ui_Bind(bar, "Center view", "center_view");
	Ui_Bind(bar, "Jump", "+move_up");
	Ui_Bind(bar, "Crouch", "+move_down");
	Ui_Bind(bar, "Run / walk", "+speed");

	TwAddSeparator(bar, "View", NULL);

	Ui_CvarRange(bar, "Sensitivity", m_sensitivity, 0.0, 6.0);
	Ui_CvarText(bar, "Invert mouse", m_invert);
	Ui_CvarText(bar, "Smooth mouse", m_interpolate);

	TwAddSeparator(bar, "Weapons", NULL);

	Ui_Bind(bar, "Shotgun", "use shotgun");
	Ui_Bind(bar, "Super shotgun", "use super shotgun");
	Ui_Bind(bar, "Machinegun", "use machinegun");
	Ui_Bind(bar, "Grenade launcher", "use grenade launcher");
	Ui_Bind(bar, "Rocket launcher", "use rocket launcher");
	Ui_Bind(bar, "Hyperblaster", "use hyperblaster");
	Ui_Bind(bar, "Lightning", "use lightning");
	Ui_Bind(bar, "Railgun", "use railgun");
	Ui_Bind(bar, "BFG-10K", "use bfg10k");

	TwAddSeparator(bar, "Combat", NULL);

	Ui_Bind(bar, "Next weapon", "weapon_next");
	Ui_Bind(bar, "Previous weapon", "weapon_prev");
	Ui_Bind(bar, "Attack", "+attack");
	Ui_Bind(bar, "Zoom", "+ZOOM");

	TwAddSeparator(bar, "Communication", NULL);

	Ui_Bind(bar, "Talk", "message_mode");
	Ui_Bind(bar, "Talk to team", "message_mode_2");
	Ui_Bind(bar, "Show scores", "score");

	TwDefine("Controls size='250 550' iconified=true");

	return bar;
}
