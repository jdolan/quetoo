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

#include "viewcontrollers/MainViewController.h"
#include "viewcontrollers/LoadingViewController.h"

static LoadingViewController *loadingViewController;
static MainViewController *mainViewController;

/**
 * @brief Initializes the user interface.
 */
void Cgui_Init(void) {

	loadingViewController = $(alloc(LoadingViewController), init);

	mainViewController = $(alloc(MainViewController), init);

	cgi.PushViewController((ViewController *) mainViewController);
}

/**
 * @brief Shuts down the user interface.
 */
void Cgui_Shutdown(void) {

	cgi.PopToViewController((ViewController *) loadingViewController);
	cgi.PopToViewController((ViewController *) mainViewController);
	cgi.PopViewController();

	release(mainViewController);
}

/**
 * @brief Updates the entire UI structure
 */
void Cgui_Update(const cl_state_t state) {

	mainViewController->state = state;

	$((ViewController *) mainViewController, viewWillAppear); // FIXME: should this be a method of MainViewController?
}

/**
 * @brief Updates the loading screen
 */
void Cgui_UpdateLoading(const cl_loading_t loading) {

	if (loading.percent == 0) {
		cgi.PushViewController((ViewController *) loadingViewController);
		$(loadingViewController, setProgress, loading);
	} else if (loading.percent == 100) {
		cgi.PopToViewController((ViewController *) mainViewController);
	} else {
		$(loadingViewController, setProgress, loading);
	}
}

/**
 * @brief Shows a dialog with Cancel/Ok buttons for user input
 */
void Cgui_DialogQuestion(const char *text, const char *cancelText, const char *okText, void (*okFunction)(void)) {
	$(mainViewController->mainView->dialog, showDialog, text, cancelText, okText, okFunction);
}
