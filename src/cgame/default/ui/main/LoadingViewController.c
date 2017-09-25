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

#pragma mark - Object

static void dealloc(Object *self) {

	LoadingViewController *this = (LoadingViewController *) self;

	release(this->mapshotImage);

	release(this->progressBar);
	release(this->progressLabel);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	LoadingViewController *this = (LoadingViewController *) self;

	// Mapshot

	this->mapshotImage = $(alloc(ImageView), initWithFrame, NULL);
	assert(this->mapshotImage);

	this->mapshotImage->view.zIndex = -1;
	this->mapshotImage->view.autoresizingMask = ViewAutoresizingFill;

	$(self->view, addSubview, (View *) this->mapshotImage);

	// Quetoo logo

	const SDL_Rect frame = MakeRect(0, 0, 480, 360);

	ImageView *logoImage = $(alloc(ImageView), initWithFrame, &frame);
	assert(logoImage);

	SDL_Surface *surface;

	if (cgi.LoadSurface("ui/loading", &surface)) {
		$(logoImage, setImageWithSurface, surface);
		SDL_FreeSurface(surface);
	} else {
		$(logoImage, setImage, NULL);
	}

	logoImage->view.alignment = ViewAlignmentMiddleCenter;
	logoImage->view.autoresizingMask = ViewAutoresizingNone;

	$(self->view, addSubview, (View *) logoImage);
	release(logoImage);

	// Progress bar background

	const SDL_Rect barFrame = MakeRect(0, 0, 0, 32);

	ImageView *progressBarBg = $(alloc(ImageView), initWithFrame, &barFrame);
	assert(progressBarBg);

	if (cgi.LoadSurface("ui/pics/progress_bar_bg", &surface)) {
		$(progressBarBg, setImageWithSurface, surface);
		SDL_FreeSurface(surface);
	} else {
		$(progressBarBg, setImage, NULL);
	}

	progressBarBg->view.alignment = ViewAlignmentBottomLeft;
	progressBarBg->view.autoresizingMask = ViewAutoresizingWidth;

	$(self->view, addSubview, (View *) progressBarBg);
	release(progressBarBg);

	// Progress bar

	this->progressBar = $(alloc(ImageView), initWithFrame, &barFrame);
	assert(this->progressBar);

	if (cgi.LoadSurface("ui/pics/progress_bar", &surface)) {
		$(this->progressBar, setImageWithSurface, surface);
		SDL_FreeSurface(surface);
	} else {
		$(this->progressBar, setImage, NULL);
	}

	this->progressBar->view.alignment = ViewAlignmentBottomLeft;
	this->progressBar->view.autoresizingMask = ViewAutoresizingNone;

	$(self->view, addSubview, (View *) this->progressBar);

	// Progress label

	this->progressLabel = $(alloc(Label), initWithText, "NULL", NULL);
	assert(this->progressLabel);

	this->progressLabel->view.alignment = ViewAlignmentBottomLeft;

	$(self->view, addSubview, (View *) this->progressLabel);
}

#pragma mark - LoadingViewController

/**
 * @fn LoadingViewController *LoadingViewController::init(LoadingViewController *self)
 *
 * @memberof LoadingViewController
 */
static LoadingViewController *init(LoadingViewController *self) {

	return (LoadingViewController *) super(ViewController, self, init);
}

/**
 * @fn void LoadingViewController::setProgress(LoadingViewController *self, const cl_loading_t loading)
 *
 * @memberof LoadingViewController
 */
static void setProgress(LoadingViewController *self, const cl_loading_t loading) {

	self->progressBar->view.frame.w = (cgi.context->window_width * (loading.percent / 100.0));

	$(self->progressLabel->text, setText, va("%d%% (%s)", loading.percent, loading.status));

	// Update mapshot if applicable

	if (loading.percent == 0 && loading.mapshot[0] != '\0') {
		SDL_Surface *surface;

		if (cgi.LoadSurface(loading.mapshot, &surface)) {
			$(self->mapshotImage, setImageWithSurface, surface);
			SDL_FreeSurface(surface);
		} else {
			$(self->mapshotImage, setImage, NULL);
		}
	}
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;

	((LoadingViewControllerInterface *) clazz->def->interface)->init = init;
	((LoadingViewControllerInterface *) clazz->def->interface)->setProgress = setProgress;
}

/**
 * @fn Class *LoadingViewController::_LoadingViewController(void)
 * @memberof LoadingViewController
 */
Class *_LoadingViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "LoadingViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(LoadingViewController);
		clazz.interfaceOffset = offsetof(LoadingViewController, interface);
		clazz.interfaceSize = sizeof(LoadingViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
