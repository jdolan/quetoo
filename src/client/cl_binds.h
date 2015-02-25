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

#ifndef __CL_BINDS_H__
#define __CL_BINDS_H__

static const char *DEFAULT_BINDS =
	"bind ` toggle_console\n"
	"bind f8 toggle_console\n"
	"bind 1 use blaster\n"
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
	"bind space +move_up\n"
	"bind c +move_down\n"
	"bind left +left\n"
	"bind right +right\n"
	"bind home center_view\n"
	"bind \"left shift\" +speed\n"

	"bind x score\n"
	"bind t message_mode\n"
	"bind y message_mode_2\n"

	"bind \"mouse 1\" +attack\n"
	"bind \"mouse 2\" +move_up\n"
	"bind \"mouse 4\" weapon_previous\n"
	"bind \"mouse 5\" weapon_next\n"

	// demo playback rate
	"bind , slow_motion\n"
	"bind . fast_forward\n"

	// score alias
	"alias +SCORE score\n"
	"alias -SCORE score\n"

	"bind tab +SCORE\n"

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

#if defined(__APPLE__)
	"bind \"left option\" +ZOOM\n"
#else
	"bind \"left alt\" +ZOOM\n"
#endif

	// screenshots alias for mappers
	"alias SCREENSHOTS \""
	"set c $cl_draw_counters;"
	"set n $cl_draw_net_graph;"
	"set x $cg_draw_crosshair;"
	"set h $cg_draw_hud;"
	"set w $cg_draw_weapon;"
	"set cl_draw_counters 0;"
	"set cl_draw_net_graph 0;"
	"set cg_draw_crosshair 0;"
	"set cg_draw_hud 0;"
	"set cg_draw_weapon 0;"
	"bind f10 SCREENSHOTS_OFF"
	"\"\n"

	"alias SCREENSHOTS_OFF \""
	"set cl_draw_counters $c;"
	"set cl_draw_net_graph $n;"
	"set cg_draw_crosshair $x;"
	"set cg_draw_hud $h;"
	"set cg_draw_weapon $w;"
	"bind f10 SCREENSHOTS"
	"\"\n"

	"bind f10 SCREENSHOTS\n"
	"bind f11 r_toggle_fullscreen\n"
	"bind f12 r_screenshot\n"

	// now execute the "default" configuration file
	"exec quetoo.cfg\n"

	// and "default" autoexec.cfg
	"exec autoexec.cfg\n";

#endif /* __CL_BINDS_H__ */
