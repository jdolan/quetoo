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

static const char *DEFAULT_BINDS =
    "bind 1 use blaster\n"
    "bind 2 use shotgun\n"
    "bind 3 use super shotgun\n"
    "bind 4 use machinegun\n"
    "bind 5 use grenade launcher\n"
    "bind 6 use rocket launcher\n"
    "bind 7 use hyperblaster\n"
    "bind 8 use lightning gun\n"
    "bind 9 use railgun\n"
    "bind 0 use bfg10k\n"
    "bind g use hand grenades\n"

    "bind w +forward\n"
    "bind a +move_left\n"
    "bind s +back\n"
    "bind d +move_right\n"
    "bind space +move_up\n"
    "bind c +move_down\n"
    "bind left +left\n"
    "bind right +right\n"
    "bind home center_view\n"
    "bind \"left shift\" +speed\n"

    "bind t cl_message_mode\n"
	//"bind enter cl_message_mode\n"
	"bind return cl_message_mode\n"
    "bind y cl_message_mode_2\n"

    "bind \"mouse 1\" +attack\n"
    "bind \"mouse 2\" +hook\n"
    "bind \"mouse 3\" +move_up\n"
    "bind \"mouse wheel up\" cg_weapon_previous\n"
    "bind \"mouse wheel down\" cg_weapon_next\n"

    // demo playback rate
    "bind , slow_motion\n"
    "bind . fast_forward\n"

	// score
    "bind tab +score\n"

    // zoom alias for nubs
    "alias +ZOOM \""
    "set f $cg_fov;"
    "set cg_fov $cg_fov_zoom;"
    "set s $m_sensitivity;"
    "set m_sensitivity $m_sensitivity_zoom;"
    "\"\n"

    "alias -ZOOM \""
    "set cg_fov $f;"
    "set m_sensitivity $s;"
    "\"\n"

    "bind \"left alt\" +ZOOM\n"

    // spam alias for pros
    "alias SPAM \""
    "use hyperblaster;"
    "use lightning gun;"
    "bind f SPAM"
    "\"\n"

	"bind f9 toggle cg_draw_weapon\n"
    "bind f10 r_screenshot view\n"
    "bind f11 r_toggle_fullscreen\n"
    "bind f12 r_screenshot\n"

    // now execute the "default" configuration file
    "exec quetoo.cfg\n"

    // and "default" autoexec.cfg
    "exec autoexec.cfg\n"

	// bind these last in case somebody is using a Quake 2 config
    "bind ` cl_toggle_console\n"
    "bind f8 cl_toggle_console\n";
