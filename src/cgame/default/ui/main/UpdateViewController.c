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

#define HERO_CYCLE_MSEC 5000
#define HERO_FADE_MSEC  1000

/**
 * @brief `Net_HttpCallback` for the initial hero image fetch.
 * Sets heroShot and schedules the first cycle.
 */
static void heroImageCallback(int32_t status, void *body, size_t length, void *user_data) {

	if (status == 200 && body && length) {
		Data *imageData = $$(Data, dataWithBytes, body, length);
		Image *image = $$(Image, imageWithData, imageData);
		release(imageData);
		if (image) {
			UpdateViewController *this = (UpdateViewController *) user_data;
			$(this->heroShot, setImage, image);
			release(image);
			this->heroCycleAt = SDL_GetTicks() + HERO_CYCLE_MSEC;
		}
	}
}

/**
 * @brief Net_HttpCallback for cycling hero image fetches.
 * Sets heroShotNext and triggers the cross-fade.
 */
static void heroNextImageCallback(int32_t status, void *body, size_t length, void *user_data) {

	if (status == 200 && body && length) {
		Data *imageData = $$(Data, dataWithBytes, body, length);
		Image *image = $$(Image, imageWithData, imageData);
		release(imageData);
		if (image) {
			UpdateViewController *this = (UpdateViewController *) user_data;
			$(this->heroShotNext, setImage, image);
			release(image);
			this->heroFadeStart = SDL_GetTicks();
		}
	}
}

/**
 * @brief Net_HttpCallback for the S3 hero-images directory listing.
 * Parses <Key> entries from the XML response, then fetches the first image.
 */
static void heroListCallback(int32_t status, void *body, size_t length, void *user_data) {

	if (status != 200 || !body || !length) {
		return;
	}

	UpdateViewController *this = (UpdateViewController *) user_data;

	GPtrArray *names = g_ptr_array_new_with_free_func(g_free);

	static const char prefix[] = "<Key>hero-images/";
	static const char suffix[] = "</Key>";
	const size_t prefix_len = sizeof(prefix) - 1;

	const char *p = (const char *) body;
	while ((p = strstr(p, prefix)) != NULL) {
		p += prefix_len;
		const char *end = strstr(p, suffix);
		if (!end) {
			break;
		}
		const size_t name_len = end - p;
		if (name_len > 0 && name_len < 128) {
			g_ptr_array_add(names, g_strndup(p, name_len));
		}
		p = end + sizeof(suffix) - 1;
	}

	if (names->len == 0) {
		g_ptr_array_free(names, true);
		return;
	}

	this->heroNames = names;
	this->heroIndex = SDL_GetTicks() % names->len;

	const char *name = g_ptr_array_index(names, this->heroIndex);
	char hero_url[256];
	g_snprintf(hero_url, sizeof(hero_url), "%s%s", QUETOO_HERO_BASE_URL, name);

	cgi.HttpGetAsync(hero_url, heroImageCallback, this);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	UpdateViewController *this = (UpdateViewController *) self;

	if (this->heroNames) {
		g_ptr_array_free(this->heroNames, true);
	}

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
		MakeOutlet("heroShot", &this->heroShot),
		MakeOutlet("heroShotNext", &this->heroShotNext),
		MakeOutlet("logo", &this->logo),
		MakeOutlet("progress", &this->progressBar)
	);

	$(self->view, awakeWithResourceName, "ui/main/UpdateViewController.json");
	$(self->view, resolve, outlets);

	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/main/UpdateViewController.css");
	assert(self->view->stylesheet);

	$(this->logo, setImageWithResourceName, "ui/loading.tga");
	$(this->progressBar->foreground, setImageWithResourceName, "ui/pics/progress_bar.tga");

	this->heroShotNext->color.a = 0;

	// Fetch the hero-images directory listing, then display a random image.
	cgi.HttpGetAsync(QUETOO_HERO_LIST_URL, heroListCallback, this);
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

	// Per-frame hero image cycling and cross-fade.
	if (self->heroNames) {
		if (self->heroFadeStart) {
			const float t = (float)(SDL_GetTicks() - self->heroFadeStart) / HERO_FADE_MSEC;
			self->heroShotNext->color.a = (Uint8)((t < 1.0f ? t : 1.0f) * 255.0f);
			if (t >= 1.0f) {
				// Fade complete: promote heroShotNext to heroShot.
				$(self->heroShot, setImage, self->heroShotNext->image);
				$(self->heroShotNext, setImage, NULL);
				self->heroShotNext->color.a = 0;
				self->heroFadeStart = 0;
				self->heroCycleAt = SDL_GetTicks() + HERO_CYCLE_MSEC;
			}
		} else if (self->heroCycleAt && SDL_GetTicks() >= self->heroCycleAt) {
			self->heroCycleAt = 0; // prevent re-triggering while fetch is in flight
			self->heroIndex = (self->heroIndex + 1) % self->heroNames->len;
			const char *name = g_ptr_array_index(self->heroNames, self->heroIndex);
			char url[256];
			g_snprintf(url, sizeof(url), "%s%s", QUETOO_HERO_BASE_URL, name);
			cgi.HttpGetAsync(url, heroNextImageCallback, self);
		}
	}

	switch (status.phase) {
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
