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
 * @brief Net_HttpCallback for the hero image fetch.
 * Called on a background thread; decodes and sets the heroShot image.
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
		}
	}
}

/**
 * @brief Net_HttpCallback for the S3 hero-images directory listing.
 * Parses <Key> entries from the XML response, picks one at random, then
 * fires heroImageCallback to fetch and display it.
 */
static void heroListCallback(int32_t status, void *body, size_t length, void *user_data) {

	if (status != 200 || !body || !length) {
		return;
	}

	// Collect filenames by scanning for <Key>hero-images/name</Key> in the XML.
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

	if (names->len > 0) {
		const char *name = g_ptr_array_index(names, SDL_GetTicks() % names->len);
		char hero_url[256];
		g_snprintf(hero_url, sizeof(hero_url), "%s%s", QUETOO_HERO_BASE_URL, name);
		cgi.HttpGetAsync(hero_url, heroImageCallback, user_data);
	}

	g_ptr_array_free(names, true);
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
		MakeOutlet("logo", &this->logo),
		MakeOutlet("progress", &this->progressBar)
	);

	$(self->view, awakeWithResourceName, "ui/main/UpdateViewController.json");
	$(self->view, resolve, outlets);

	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/main/UpdateViewController.css");
	assert(self->view->stylesheet);

	$(this->logo, setImageWithResourceName, "ui/loading.tga");
	$(this->progressBar->foreground, setImageWithResourceName, "ui/pics/progress_bar.tga");

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
static void setStatus(UpdateViewController *self, installer_state_t status) {

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
