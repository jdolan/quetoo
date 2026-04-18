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

#include <Objectively/URLSession.h>

#include "UpdateViewController.h"

#define _Class _UpdateViewController

#define QUETOO_HERO_BASE_URL "https://quetoo.s3.amazonaws.com/hero-images/"

static const char *hero_images[] = {
	"chthon05.jpg",
	"fraggin05.jpg",
	"longyard01.jpg",
	"quetoo000.jpg",
	"quetoo014.jpg",
	"quetoo019.jpg",
	"quetoo020.jpg",
	"quetoo022.jpg",
	"quetoo024.jpg",
	"quetoo027.jpg",
	"quetoo030.jpg",
	"quetoo033.jpg",
	"quetoo035.jpg",
	"quetoo038.jpg",
};

#pragma mark - URLSession completion

/**
 * @brief URLSessionTaskCompletion for the hero image fetch.
 * Called on a background thread; sets the heroShot image when data arrives.
 */
static void heroImageCompletion(URLSessionTask *task, bool success) {

	if (success) {
		URLSessionDataTask *dataTask = (URLSessionDataTask *) task;
		Image *image = $$(Image, imageWithData, dataTask->data);
		if (image) {
			UpdateViewController *this = task->data;
			$(this->heroShot, setImage, image);
			release(image);
		}
	}
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

	// Fetch a random hero image asynchronously via URLSession.
	const int n = sizeof(hero_images) / sizeof(hero_images[0]);
	const char *name = hero_images[SDL_GetTicks() % n];

	char hero_url[256];
	g_snprintf(hero_url, sizeof(hero_url), "%s%s", QUETOO_HERO_BASE_URL, name);

	URL *url = $(alloc(URL), initWithCharacters, hero_url);
	URLSession *session = $$(URLSession, sharedInstance);
	URLSessionDataTask *task = $(session, dataTaskWithURL, url, heroImageCompletion);
	task->urlSessionTask.data = self;
	$((URLSessionTask *) task, resume);
	release(task);
	release(url);
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
