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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

/**
 * @brief The binds the engine owns. The client game binds the rest, with
 * `Cg_BindKeys`, once it has loaded.
 */
static const char *DEFAULT_BINDS =
    "bind f11 r_screenshot view\n"
    "bind f12 r_screenshot\n"

    // now execute the "default" configuration file
    "exec quetoo.cfg\n"

    // bind these last in case somebody is using a Quake 2 config
    "bind ` cl_toggleConsole\n"
    "bind f8 cl_toggleConsole\n";
