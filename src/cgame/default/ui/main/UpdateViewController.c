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

#include "UpdateViewController.h"

#define _Class _UpdateViewController

#define QUETOO_HERO_BASE_URL  "https://quetoo.s3.amazonaws.com/hero-images/"
#define QUETOO_HERO_LIST_URL  "https://quetoo.s3.amazonaws.com/?prefix=hero-images/&delimiter=/"

/**
 * @brief `ThreadRunFunc` that pre-fetches all hero images synchronously.
 */
static void fetchHeroImages(void *data) {

	UpdateViewController *this = data;

	void *list_body = NULL, *image_body = NULL;
	size_t list_length, image_length;

	if (cgi.HttpGet(QUETOO_HERO_LIST_URL, &list_body, &list_length) != 200) {
		Cg_Warn("Failed to fetch hero image list");
		cgi.Free(list_body);
		return;
	}

	static const char prefix[] = "<Key>hero-images/";
	static const char suffix[] = "</Key>";

	char *p = (char *) list_body;
	while ((p = strstr(p, prefix)) != NULL) {

		p += strlen(prefix);
		char *s = strstr(p, suffix);
		assert(s);
		*s = '\0';

		char url[MAX_STRING_CHARS];
		const int url_len = g_snprintf(url, sizeof(url), "%s%s", QUETOO_HERO_BASE_URL, p);
		if (url_len < 0 || (size_t) url_len >= sizeof(url)) {
			Cg_Warn("Failed to build hero image URL: %s", p);
			p = s + strlen(suffix);
			continue;
		}
		if (cgi.HttpGet(url, &image_body, &image_length) == 200) {
			Image *image = $$(Image, imageWithBytes, image_body, image_length);
			if (image) {
				SDL_LockMutex(this->pendingImagesLock);
				$(this->pendingImages, addObject, image);
				SDL_UnlockMutex(this->pendingImagesLock);
			}
			release(image);
		} else {
			Cg_Warn("Failed to fetch hero image: %s", url);
		}

		cgi.Free(image_body);
		p = s + strlen(suffix);
	}

	cgi.Free(list_body);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  UpdateViewController *this = (UpdateViewController *) self;

  SDL_DestroyMutex(this->pendingImagesLock);
  release(this->pendingImages);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	UpdateViewController *this = (UpdateViewController *) self;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("slideShow", &this->slideShow),
		MakeOutlet("logo", &this->logo),
		MakeOutlet("progress", &this->progressBar)
	);

	$(self->view, awakeWithResourceName, "ui/main/UpdateViewController.json");
	$(self->view, resolve, outlets);

	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/main/UpdateViewController.css");
	assert(self->view->stylesheet);

	this->pendingImagesLock = SDL_CreateMutex();
	assert(this->pendingImagesLock);
	this->pendingImages = $(alloc(MutableArray), init);
	assert(this->pendingImages);

	$(this->logo, setImageWithResourceName, "ui/loading.tga");
	$(this->progressBar->foreground, setImageWithResourceName, "ui/progress_bar.tga");

	cgi.Thread(__func__, fetchHeroImages, this, THREAD_NO_WAIT);
}

#pragma mark - UpdateViewController

/**
 * @fn UpdateViewController *UpdateViewController::init(UpdateViewController *self)
 * @memberof UpdateViewController
 */
static UpdateViewController *init(UpdateViewController *self) {
	return (UpdateViewController *) super(ViewController, self, init);
}

/**
 * @fn void UpdateViewController::setStatus(UpdateViewController *self, installer_state_t status)
 * @memberof UpdateViewController
 */
static void setStatus(UpdateViewController *self, installer_status_t status) {

	SDL_LockMutex(self->pendingImagesLock);
	const Array *pending = (Array *) self->pendingImages;
	for (size_t i = 0; i < pending->count; i++) {
		Image *image = $(pending, objectAtIndex, i);
		$(self->slideShow, addImage, image);
	}
	$(self->pendingImages, removeAllObjects);
	SDL_UnlockMutex(self->pendingImagesLock);

	switch (status.state) {
		case INSTALLER_CHECKING:
			$(self->progressBar, setLabelFormat, "Checking for updates\u2026");
			$(self->progressBar, setValue, 0.0);
			break;
		case INSTALLER_LISTING:
			$(self->progressBar, setLabelFormat, "Listing files\u2026");
			$(self->progressBar, setValue, 0.0);
			break;
		case INSTALLER_DONE:
			$(self->progressBar, setLabelFormat, "Data is up to date.");
			$(self->progressBar, setValue, 100.0);
			break;
		case INSTALLER_ERROR:
			$(self->progressBar, setLabelFormat, va("Error: %s", status.error));
			$(self->progressBar, setValue, 0.0);
			break;
		default: {
			double pct = 0.0;
			if (status.kbytes_total > 0) {
				pct = 100.0 * status.kbytes_done / status.kbytes_total;
			} else if (status.files_total > 0) {
				pct = 100.0 * status.files_done / status.files_total;
			}
			const char *label = status.current_file[0]
				? va("Updating %s (%d / %d)\u2026", status.current_file, status.files_done, status.files_total)
				: "Downloading updates\u2026";
			$(self->progressBar, setLabelFormat, label);
			$(self->progressBar, setValue, pct);
			break;
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

	((UpdateViewControllerInterface *) clazz->interface)->init = init;
	((UpdateViewControllerInterface *) clazz->interface)->setStatus = setStatus;
}

/**
 * @fn Class *UpdateViewController::_UpdateViewController(void)
 * @memberof UpdateViewController
 */
Class *_UpdateViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "UpdateViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(UpdateViewController),
			.interfaceOffset = offsetof(UpdateViewController, interface),
			.interfaceSize = sizeof(UpdateViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
