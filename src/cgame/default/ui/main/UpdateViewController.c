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
 * @brief `Net_HttpCallback` for each pre-fetched hero image.
 * Decodes and appends the image to heroImages; shows the first one immediately.
 */
static void heroImageCallback(int32_t status, void *body, size_t length, void *user_data) {

	if (status == 200) {
		Image *image = $$(Image, imageWithBytes, body, length);
    assert(image);

    UpdateViewController *this = (UpdateViewController *) user_data;
    $(this->heroImages, addObject, image);

    release(image);

    if (this->heroImages->array.count == 1) {
      $(this->heroImage, setImage, image);
      this->heroCycleAt = SDL_GetTicks() + HERO_CYCLE_MSEC;
    }
  } else {
    Cg_Warn("Failed to fetch hero image: %d", status);
  }
}

/**
 * @brief `Net_HttpCallback` for the S3 hero-images directory listing.
 * Fires one `HttpGetAsync` per image to pre-fetch the full set into `heroImages`.
 */
static void heroListCallback(int32_t status, void *body, size_t length, void *user_data) {

  const char prefix[] = "<Key>hero-images/";
  const char suffix[] = "</Key>";

  UpdateViewController *this = (UpdateViewController *) user_data;

  if (status == 200) {
    char *p = (char *) body;
    while ((p = strstr(p, prefix)) != NULL) {
      char *name = p + strlen(prefix);
      char *s = strstr(name, suffix);
      if (!s) {
        Cg_Warn("Malformed hero image list");
        break;
      }
      *s = '\0';
      cgi.HttpGetAsync(va("%s%s", QUETOO_HERO_BASE_URL, name), heroImageCallback, this);
      p = s + strlen(suffix);
    }
  } else {
    Cg_Warn("Failed to fetch hero image list: %d", status);
  }
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	UpdateViewController *this = (UpdateViewController *) self;

	release(this->heroImages);

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
		MakeOutlet("heroShot", &this->heroImage),
		MakeOutlet("heroShotNext", &this->nextHeroImage),
		MakeOutlet("logo", &this->logo),
		MakeOutlet("progress", &this->progressBar)
	);

	$(self->view, awakeWithResourceName, "ui/main/UpdateViewController.json");
	$(self->view, resolve, outlets);

	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/main/UpdateViewController.css");
	assert(self->view->stylesheet);

	$(this->logo, setImageWithResourceName, "ui/loading.tga");
	$(this->progressBar->foreground, setImageWithResourceName, "ui/pics/progress_bar.tga");

	this->heroImages = $(alloc(MutableArray), init);
	this->nextHeroImage->color.a = 0;

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

	UpdateViewController *this = self;

	const size_t heroCount = this->heroImages->array.count;
	if (heroCount > 1) {
		if (this->heroFadeStart) {
			const float t = (float) (SDL_GetTicks() - this->heroFadeStart) / HERO_FADE_MSEC;
			this->nextHeroImage->color.a = (Uint8) (MIN(t, 1.0f) * 255.0f);
			if (t >= 1.0f) {
				$(this->heroImage, setImage, this->nextHeroImage->image);
				$(this->nextHeroImage, setImage, NULL);
				this->nextHeroImage->color.a = 0;
				this->heroFadeStart = 0;
				this->heroCycleAt = SDL_GetTicks() + HERO_CYCLE_MSEC;
			}
		} else if (this->heroCycleAt && SDL_GetTicks() >= this->heroCycleAt) {
			this->heroCycleAt = 0;
			this->heroIndex = (this->heroIndex + 1) % heroCount;
			Image *next = $((Array *) this->heroImages, objectAtIndex, this->heroIndex);
			$(this->nextHeroImage, setImage, next);
			this->heroFadeStart = SDL_GetTicks();
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
