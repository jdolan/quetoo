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

#include <SDL3_image/SDL_image.h>
#include <Objectively/PointerArray.h>

#include "UpdateViewController.h"

#define _Class _UpdateViewController

#define QUETOO_HERO_BASE_URL  "https://quetoo.s3.amazonaws.com/hero-images/"
#define QUETOO_HERO_LIST_URL  "https://quetoo.s3.amazonaws.com/?prefix=hero-images/&delimiter=/"

#pragma mark - Delegates

/**
 * @brief `ThreadRunFunc` that pre-fetches all hero images synchronously.
 */
static void fetchHeroImages(void *data) {

	UpdateViewController *this = data;

	Data *list_data = NULL, *image_data = NULL;

	if ($(cgi.restClient, get, QUETOO_HERO_LIST_URL, &list_data) != 200 || !list_data) {
		Cg_Warn("Failed to fetch hero image list");
		release(list_data);
		return;
	}

	static const char prefix[] = "<Key>hero-images/";
	static const char suffix[] = "</Key>";

	PointerArray *urls = $(alloc(PointerArray), initWithDestroy, free);

	char *p = (char *) list_data->bytes;
	while ((p = q_strstr(p, prefix)) != NULL) {

		p += q_strlen(prefix);
		char *s = q_strstr(p, suffix);
		assert(s);
		*s = '\0';

		if (*p == '\0') {
			p = s + q_strlen(suffix);
			continue;
		}

		char url[MAX_STRING_CHARS];
		const int url_len = q_snprintf(url, sizeof(url), "%s%s", QUETOO_HERO_BASE_URL, p);
		if (url_len < 0 || (size_t) url_len >= sizeof(url)) {
			Cg_Warn("Failed to build hero image URL: %s", p);
			p = s + q_strlen(suffix);
			continue;
		}

		$(urls, add, q_strdup(url));
		p = s + q_strlen(suffix);
	}

	release(list_data);

	if (urls->count > 1) {
		for (size_t i = urls->count - 1; i > 0; i--) {
			const size_t j = Randomu() % (i + 1);
			void *tmp = urls->elements[i];
			urls->elements[i] = urls->elements[j];
			urls->elements[j] = tmp;
		}
	}

	for (size_t i = 0; i < urls->count; i++) {
		image_data = NULL;
		const char *url_str = urls->elements[i];
		if ($(cgi.restClient, get, url_str, &image_data) == 200 && image_data) {
			Image *image = NULL;

      SDL_Surface *surf = cgi.LoadSurfaceFromData(image_data->bytes, image_data->length);
      if (surf) {
        cgi.BlurSurface(surf, 3);
        image = $$(Image, imageWithSurface, surf);
        SDL_DestroySurface(surf);
      }

			if (image == NULL) {
				image = $$(Image, imageWithBytes, image_data->bytes, image_data->length);
			}

			if (image) {
				SDL_LockMutex(this->pendingImagesLock);
				$(this->pendingImages, addObject, image);
				SDL_UnlockMutex(this->pendingImagesLock);
			}
			release(image);
		} else {
			Cg_Warn("Failed to fetch hero image: %s", url_str);
		}
		release(image_data);
	}

	urls = release(urls);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  UpdateViewController *this = (UpdateViewController *) self;

  cgi.Wait(this->fetchThread);

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
	this->pendingImages = $(alloc(Array), init);
	assert(this->pendingImages);

	$(this->logo, setImageWithResourceName, "ui/loading.png");
	$(this->progressBar->foreground, setImageWithResourceName, "ui/progress_bar.png");

	this->fetchThread = cgi.Thread(__func__, fetchHeroImages, this, THREAD_NO_WAIT);
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
 * @fn void UpdateViewController::setStatus(UpdateViewController *self, const installer_state_t status)
 * @memberof UpdateViewController
 */
static void setStatus(UpdateViewController *self, const installer_status_t *in) {

	SDL_LockMutex(self->pendingImagesLock);
	const Array *pending = (Array *) self->pendingImages;
	if (pending->count) {
		for (size_t i = 0; i < pending->count; i++) {
			Image *image = $(pending, objectAtIndex, i);
			$(self->slideShow, addImage, image);
		}
		$(self->pendingImages, removeAllObjects);
	}
	SDL_UnlockMutex(self->pendingImagesLock);

	switch (in->state) {
    case INSTALLER_CHECKING:
      $(self->progressBar, setLabelFormat, "Checking for binary updates\u2026");
      $(self->progressBar, setValue, 0.0);
      break;
    case INSTALLER_UPDATE_AVAILABLE:
      $(self->progressBar, setLabelFormat, "Update available.");
      $(self->progressBar, setValue, 100.0);
      break;
		case INSTALLER_COMPARING:
			$(self->progressBar, setLabelFormat, "Comparing data files\u2026");
			$(self->progressBar, setValue, 0.0);
			break;
		case INSTALLER_COMMITTING:
			$(self->progressBar, setLabelFormat, "Committing update\u2026");
			$(self->progressBar, setValue, 99.0);
			break;
		case INSTALLER_DONE:
			$(self->progressBar, setLabelFormat, "Data is up to date.");
			$(self->progressBar, setValue, 100.0);
			break;
		case INSTALLER_ERROR:
			$(self->progressBar, setLabelFormat, in->error);
			$(self->progressBar, setValue, 0.0);
			break;
		case INSTALLER_DOWNLOADING: {
			double pct = 0.0;
			if (in->kbytes_total > 0) {
				pct = 100.0 * in->kbytes_done / in->kbytes_total;
			} else if (in->files_total > 0) {
				pct = 100.0 * in->files_done / in->files_total;
			}
			const char *label = va("Downloading (%d / %d) %s \u2026", in->files_done, in->files_total, in->current_file);
			$(self->progressBar, setLabelFormat, label);
			$(self->progressBar, setValue, pct);
		}
      break;
    default:
      break;
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
