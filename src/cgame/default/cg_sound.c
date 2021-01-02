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

#include "cg_local.h"

/**
 * @brief Updates the sound stage from the interpolated frame.
 */
void Cg_PrepareStage(const cl_frame_t *frame) {

	cgi.stage->ticks = cgi.view->ticks;
	cgi.stage->origin = cgi.view->origin;
	cgi.stage->angles = cgi.view->angles;
	cgi.stage->forward = cgi.view->forward;
	cgi.stage->right = cgi.view->right;
	cgi.stage->up = cgi.view->up;
	cgi.stage->contents = cgi.view->contents;
}
