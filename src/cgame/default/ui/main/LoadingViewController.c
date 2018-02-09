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

#include "QuetooTheme.h"

#define _Class _LoadingViewController

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	LoadingViewController *this = (LoadingViewController *) self;

	release(this->mapShot);
	release(this->logo);
	release(this->progressBar);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	LoadingViewController *this = (LoadingViewController *) self;

	QuetooTheme *theme = $(alloc(QuetooTheme), initWithTarget, self->view);
	assert(theme);

	this->mapShot = $(alloc(ImageView), initWithFrame, NULL);
	assert(this->mapShot);

	this->mapShot->view.autoresizingMask = ViewAutoresizingFill;

	$(theme, attach, this->mapShot);

	this->logo = $(theme, image, "ui/loading", &MakeRect(0, 0, 480, 360));
	this->logo->view.alignment = ViewAlignmentMiddleCenter;

	$(theme, attach, this->logo);

	this->progressBar = $(alloc(ProgressBar), initWithFrame, &MakeRect(0, 0, 0, 32));
	assert(this->progressBar);

	this->progressBar->label->view.alignment = ViewAlignmentMiddleLeft;

	this->progressBar->view.alignment = ViewAlignmentBottomCenter;
	this->progressBar->view.autoresizingMask = ViewAutoresizingWidth;

	SDL_Surface *surface;
	if (cgi.LoadSurface("ui/pics/progress_bar", &surface)) {
		$(this->progressBar->foreground, setImageWithSurface, surface);
		SDL_FreeSurface(surface);
	} else {
		$(this->progressBar->foreground, setImage, NULL);
	}

	$(theme, attach, this->progressBar);

	release(theme);
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

		SDL_Surface *surface;
		if (cgi.LoadSurface(loading.mapshot, &surface)) {
			$(self->mapShot, setImageWithSurface, surface);
			SDL_FreeSurface(surface);
		} else {
			$(self->mapShot, setImage, NULL);
		}
	}
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

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
