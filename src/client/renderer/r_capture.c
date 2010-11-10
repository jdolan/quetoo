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

#include "renderer.h"


/*
 * R_UpdateCapture
 *
 * Captures the current frame buffer to memory.  JPEG encoding is optionally
 * processed in a separate thread for performance reasons.  See below.
 */
void R_UpdateCapture(void){
	int w, h;
	byte *b;

	if(r_view.update || r_capture->modified){
		r_capture->modified = false;

		R_ShutdownCapture();
		R_InitCapture();
	}

	if(!r_capture->value)
		return;

	w = r_state.width;
	h = r_state.height;

	b = r_locals.capturebuffer;

	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, b);
}


/*
 * R_FlushCapture
 *
 * Performs the JPEG file authoring for the most recently captured frame.
 */
void R_FlushCapture(void){
	char path[MAX_OSPATH];
	int f, w, h, q;
	byte *b;

	if(!r_capture->value)
		return;

	f = r_locals.captureframe++;

	snprintf(path, sizeof(path), "%s/capture/%010d.jpg", Fs_Gamedir(), f);

	b = r_locals.capturebuffer;

	w = r_state.width;
	h = r_state.height;

	q = r_capture_quality->value * 100;

	if(q < 1)
		q = 1;
	else if(q > 100)
		q = 100;

	Img_WriteJPEG(path, b, w, h, q);
}


/*
 * R_InitCapture
 */
void R_InitCapture(void){
	char path[MAX_OSPATH];
	const size_t size = r_state.width * r_state.height * 3;

	r_locals.capturebuffer = Z_Malloc(size);
	r_locals.captureframe = 0;

	// ensure the capture directory exists
	snprintf(path, sizeof(path), "%s/capture/", Fs_Gamedir());
	Fs_CreatePath(path);
}


/*
 * R_ShutdownCapture
 */
void R_ShutdownCapture(void){

	if(r_locals.capturebuffer)
		Z_Free(r_locals.capturebuffer);
}
