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

#ifndef __CL_BINDS_H__
#define __CL_BINDS_H__

static const char *DEFAULT_BINDS =

"bind 2 use shotgun\n"
"bind 3 use super shotgun\n"
"bind 4 use machinegun\n"
"bind 5 use grenade launcher\n"
"bind 6 use rocket launcher\n"
"bind 7 use hyperblaster\n"
"bind 8 use lightning\n"
"bind 9 use railgun\n"
"bind 0 use bfg10k\n"

"bind - view_size_down\n"
"bind = view_size_up\n"

"bind w +forward\n"
"bind a +move_left\n"
"bind s +back\n"
"bind d +move_right\n"
"bind SPACE +move_up\n"
"bind c +move_down\n"
"bind LEFTARROW +left\n"
"bind RIGHTARROW +right\n"
"bind HOME center_view\n"
"bind SHIFT +speed\n"
"bind CTRL +attack\n"

"bind x score\n"
"bind t message_mode\n"
"bind y message_mode_2\n"

"bind TAB score\n"
"bind F12 r_screenshot\n"

"bind MOUSE1 +attack\n"
"bind MOUSE2 +move_up\n"
"bind MWHEELUP weapon_previous\n"
"bind MWHEELDOWN weapon_next\n"

// demo playback rate
"bind LEFTARROW slow_motion\n"
"bind RIGHTARROW fast_forward\n"

// zoom alias for nubs
"alias +ZOOM \""
	"set f $cl_fov;"
	"set cl_fov $cl_fov_zoom;"
	"set s $m_sensitivity;"
	"set m_sensitivity $m_sensitivity_zoom;"
	"\"\n"

"alias -ZOOM \""
	"set cl_fov $f;"
	"set m_sensitivity $s;"
	"\"\n"

"bind ALT +ZOOM\n"

// screenshots alias for mappers
"alias SCREENSHOTS \""
	"set c $cl_counters;"
	"set ch $cl_crosshair;"
	"set h $cl_hud;"
	"set n $cl_net_graph;"
	"set w $cl_weapon;"
	"set cl_counters 0;"
	"set cl_crosshair 0;"
	"set cl_hud 0;"
	"set cl_net_graph 0;"
	"set cl_weapon 0;"
	"bind F10 SCREENSHOTS_OFF"
	"\"\n"

"alias SCREENSHOTS_OFF \""
	"set cl_counters $c;"
	"set cl_crosshair $ch;"
	"set cl_hud $h;"
	"set cl_net_graph $n;"
	"set cl_weapon $w;"
	"bind F10 SCREENSHOTS"
	"\"\n"

"bind F10 SCREENSHOTS\n";

#endif /* __CL_BINDS_H__ */
