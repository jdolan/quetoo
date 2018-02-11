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

#include "LoadingViewController.h"

#define _Class _LoadingViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	LoadingViewController *this = (LoadingViewController *) self;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("mapShot", &this->mapShot),
		MakeOutlet("logo", &this->logo),
		MakeOutlet("progress", &this->progressBar)
	);

	cgi.WakeView(self->view, "ui/main/LoadingViewController.json", outlets);

	self->view->stylesheet = cgi.Stylesheet("ui/main/LoadingViewController.css");
	assert(self->view->stylesheet);

	cgi.SetImage(this->logo, "ui/loading");
	cgi.SetImage(this->progressBar->foreground, "ui/pics/progress_bar");
}

#pragma mark - LoadingViewController

/**
 * @fn LoadingViewController *LoadingViewController::init(LoadingViewController *self)
 * @memberof LoadingViewController
 */
static LoadingViewController *init(LoadingViewController *self) {
	return (LoadingViewController *) super(ViewController, self, init);
}

/**
 * @fn void LoadingViewController::setProgress(LoadingViewController *self, const cl_loading_t loading)
 * @memberof LoadingViewController
 */
static void setProgress(LoadingViewController *self, const cl_loading_t loading) {

	$(self->progressBar, setLabelFormat, va("%%0.0lf%%%% (%s)", loading.status));
	$(self->progressBar, setValue, loading.percent);

	if (loading.percent == 0 && loading.mapshot[0] != '\0') {
		cgi.SetImage(self->mapShot, loading.mapshot);
	}
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;

	((LoadingViewControllerInterface *) clazz->interface)->init = init;
	((LoadingViewControllerInterface *) clazz->interface)->setProgress = setProgress;
}

/**
 * @fn Class *LoadingViewController::_LoadingViewController(void)
 * @memberof LoadingViewController
 */
Class *_LoadingViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "LoadingViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(LoadingViewController),
			.interfaceOffset = offsetof(LoadingViewController, interface),
			.interfaceSize = sizeof(LoadingViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
