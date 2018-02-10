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

#include "ui/main/MainViewController.h"
#include "ui/main/LoadingViewController.h"

static MainViewController *mainViewController;
static Stylesheet *stylesheet;

/**
 * @brief Initializes the user interface.
 */
void Cg_InitUi(void) {

	stylesheet = cgi.Stylesheet("ui/common/common.css");
	assert(stylesheet);

	$(cgi.Theme(), addStylesheet, stylesheet);

	mainViewController = $(alloc(MainViewController), init);
	assert(mainViewController);

	cgi.PushViewController((ViewController *) mainViewController);
}

/**
 * @brief Shuts down the user interface.
 */
void Cg_ShutdownUi(void) {

	cgi.PopAllViewControllers();

	release(mainViewController);
	mainViewController = NULL;

	$(cgi.Theme(), removeStylesheet, stylesheet);

	release(stylesheet);
	stylesheet = NULL;
}

/**
 * @brief Updates the loading screen
 */
void Cg_UpdateLoading(const cl_loading_t loading) {
	static LoadingViewController *loadingViewController;

	if (loading.percent == 0) {
		loadingViewController = $(alloc(LoadingViewController), init);
		cgi.PushViewController((ViewController *) loadingViewController);
	} else if (loading.percent == 100) {
		cgi.PopToViewController((ViewController *) mainViewController);
		loadingViewController = release(loadingViewController);
	}

	if (loadingViewController) {
		$(loadingViewController, setProgress, loading);
	}
}
