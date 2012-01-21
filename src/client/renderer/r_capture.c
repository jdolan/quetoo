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

#include "r_local.h"

typedef struct capture_buffer_s {
	byte *buffer; // the buffer for RGB pixel data
	int frame; // the last capture frame
	int time; // the time of the last capture frame
} r_capture_buffer_t;

static r_capture_buffer_t capture_buffer;

/*
 * R_FlushCapture
 *
 * Performs the JPEG file authoring for the most recently captured frame.
 */
static void R_FlushCapture(void *data) {
	char path[MAX_OSPATH];
	int q;

	snprintf(path, sizeof(path), "%s/capture/%010d.jpg", Fs_Gamedir(), capture_buffer.frame);

	q = r_capture_quality->value * 100;

	if (q < 1)
		q = 1;
	else if (q > 100)
		q = 100;

	Img_WriteJPEG(path, capture_buffer.buffer, r_context.width,
			r_context.height, q);
}

/*
 * R_UpdateCapture
 *
 * Captures the current frame buffer to memory.  JPEG encoding is optionally
 * processed in a separate thread for performance reasons.  See above.
 */
void R_UpdateCapture(void) {
	static thread_t *capture_thread;

	if (!r_capture->value)
		return;

	if (r_view.time < capture_buffer.time)
		capture_buffer.time = 0;

	// enforce the capture frame rate
	if (r_view.time - capture_buffer.time < 1000.0 / r_capture_fps->value)
		return;

	Thread_Wait(&capture_thread);

	if (r_view.update || r_capture->modified) {
		r_capture->modified = false;

		R_ShutdownCapture();

		R_InitCapture();
	}

	glReadPixels(0, 0, r_context.width, r_context.height, GL_RGB,
			GL_UNSIGNED_BYTE, capture_buffer.buffer);

	capture_buffer.time = r_view.time;
	capture_buffer.frame++;

	capture_thread = Thread_Create(R_FlushCapture, NULL);
}

/*
 * R_InitCapture
 */
void R_InitCapture(void) {
	char path[MAX_OSPATH];

	// ensure the capture directory exists
	snprintf(path, sizeof(path), "%s/capture/", Fs_Gamedir());

	Fs_CreatePath(path);

	// clear the existing capture buffer
	memset(&capture_buffer, 0, sizeof(capture_buffer));

	// allocate the actual buffer
	capture_buffer.buffer = Z_Malloc(r_context.width * r_context.height * 3);
}

/*
 * R_ShutdownCapture
 */
void R_ShutdownCapture(void) {

	if (capture_buffer.buffer)
		Z_Free(capture_buffer.buffer);
}
