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

#include "ui_local.h"
#include "client.h"

#include "editor/EditorViewController.h"

extern cl_static_t cls;

static EditorViewController *editorViewController;

/**
 * @brief
 */
void Ui_CheckEditor(void) {

	if (cl_editor->modified) {

		if (cl_editor->integer) {
			if (cls.state != CL_ACTIVE) {
				return;
			}

			if (editorViewController == NULL) {
				editorViewController = $(alloc(EditorViewController), init);
			}

			Ui_PushViewController((ViewController *) editorViewController);
		} else if (editorViewController) {
			Ui_PopViewController();
		}

		cl_editor->modified = false;
	}
}

/**
 * @brief Initializes the editor.
 */
void Ui_InitEditor(void) {

	editorViewController = NULL;
}

/**
 * @brief Shuts down the editor.
 */
void Ui_ShutdownEditor(void) {

	release(editorViewController);
}
