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

#include "VideoViewController.h"

#include "CvarSelect.h"
#include "Theme.h"

#define _Class _VideoViewController

#pragma mark - Actions and delegate callbacks

/**
 * @brief SelectDelegate callback for Video Mode.
 */
static void didSelecVideoMode(Select *select, Option *option) {

	const SDL_DisplayMode *mode = option->value;

	const int32_t w = mode ? mode->w : 0;
	const int32_t h = mode ? mode->h : 0;

	if (cgi.CvarValue("r_fullscreen")) {
		cgi.CvarSetValue("r_width", w);
		cgi.CvarSetValue("r_height", h);
	} else {
		cgi.CvarSetValue("r_windowed_width", w);
		cgi.CvarSetValue("r_windowed_height", h);
	}
}

/**
 * @brief SelectDelegate callback for Quality.
 */
static void didSelectQuality(Select *select, Option *option) {

	switch ((intptr_t) option->value) {
		case 3:
			cgi.CvarSetValue("r_caustics", 1.0);
			cgi.CvarSetValue("r_shadows", 3.0);
			cgi.CvarSetValue("r_stainmaps", 1.0);
			cgi.CvarSetValue("cg_add_weather", 1.0);
			break;
		case 2:
			cgi.CvarSetValue("r_caustics", 1.0);
			cgi.CvarSetValue("r_shadows", 2.0);
			cgi.CvarSetValue("r_stainmaps", 1.0);
			cgi.CvarSetValue("cg_add_weather", 1.0);
			break;
		case 1:
			cgi.CvarSetValue("r_caustics", 0.0);
			cgi.CvarSetValue("r_shadows", 0.0);
			cgi.CvarSetValue("r_stainmaps", 1.0);
			cgi.CvarSetValue("cg_add_weather", 1.0);
			break;
		case 0:
			cgi.CvarSetValue("r_caustics", 0.0);
			cgi.CvarSetValue("r_shadows", 0.0);
			cgi.CvarSetValue("r_stainmaps", 0.0);
			cgi.CvarSetValue("cg_add_weather", 0.0);
			break;
	}
}

/**
 * @brief ActionFunction for the Apply button.
 */
static void applyAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	cgi.Cbuf("r_restart");
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	VideoViewController *this = (VideoViewController *) self;

	release(this->videoModeSelect);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Video");

	VideoViewController *this = (VideoViewController *) self;

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 2);

	$(theme, attach, columns);
	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Display");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		this->videoModeSelect = $(alloc(VideoModeSelect), initWithFrame, NULL, ControlStyleDefault);
		assert(this->videoModeSelect);

		this->videoModeSelect->select.delegate.self = this;
		this->videoModeSelect->select.delegate.didSelectOption = didSelecVideoMode;

		$(theme, control, "Video mode", this->videoModeSelect);

		Select *fullscreenSelect = (Select *) $(alloc(CvarSelect), initWithVariableName, "r_fullscreen");

		$(fullscreenSelect, addOption, "Windowed", (ident) 0);
		$(fullscreenSelect, addOption, "Fullscreen", (ident) 1);
		$(fullscreenSelect, addOption, "Borderless windowed", (ident) 2);

		$(theme, control, "Window mode", fullscreenSelect);
		release(fullscreenSelect);

		Select *swapIntervalSelect = (Select *) $(alloc(CvarSelect), initWithVariableName, "r_swap_interval");

		$(swapIntervalSelect, addOption, "On", (ident) 1);
		$(swapIntervalSelect, addOption, "Off", (ident) 0);
		$(swapIntervalSelect, addOption, "Adaptive", (ident) -1);

		$(theme, control, "Vertical sync", swapIntervalSelect);
		release(swapIntervalSelect);

		$(theme, slider, "Framerate limiter", "cl_max_fps", 0.0, 240.0, 5.0);

		release(box);
	}

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Rendering");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		Select *anisotropySelect = (Select *) $(alloc(CvarSelect), initWithVariableName, "r_anisotropy");

		$(anisotropySelect, addOption, "16x", (ident) 16);
		$(anisotropySelect, addOption, "8x", (ident) 8);
		$(anisotropySelect, addOption, "4x", (ident) 4);
		$(anisotropySelect, addOption, "2x", (ident) 2);
		$(anisotropySelect, addOption, "Off", (ident) 0);

		$(theme, control, "Anisotropy", anisotropySelect);
		release(anisotropySelect);

		Select *multisampleSelect = (Select *) $(alloc(CvarSelect), initWithVariableName, "r_multisample");

		$(multisampleSelect, addOption, "8x", (ident) 4);
		$(multisampleSelect, addOption, "4x", (ident) 2);
		$(multisampleSelect, addOption, "2x", (ident) 1);
		$(multisampleSelect, addOption, "Off", (ident) 0);

		$(theme, control, "Multisample", multisampleSelect);
		release(multisampleSelect);

		release(box);
	}

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Picture");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, slider, "Brightness", "r_brightness", 0.1, 2.0, 0.1);
		$(theme, slider, "Contrast", "r_contrast", 0.1, 2.0, 0.1);
		$(theme, slider, "Gamma", "r_gamma", 0.1, 2.0, 0.1);
		$(theme, slider, "Modulate", "r_modulate", 0.1, 5.0, 0.1);

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Quality");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		Select *qualitySelect = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

		$(qualitySelect, addOption, "Highest", (ident) 3);
		$(qualitySelect, addOption, "High", (ident) 2);
		$(qualitySelect, addOption, "Medium", (ident) 1);
		$(qualitySelect, addOption, "Low", (ident) 0);

		qualitySelect->delegate.self = this;
		qualitySelect->delegate.didSelectOption = didSelectQuality;

		$(theme, control, "Quality", qualitySelect);
		release(qualitySelect);

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Effects");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		Select *shadowsSelect = (Select *) $(alloc(CvarSelect), initWithVariableName, "r_shadows");

		$(shadowsSelect, addOption, "Highest", (ident) 3);
		$(shadowsSelect, addOption, "High", (ident) 2);
		$(shadowsSelect, addOption, "Low", (ident) 1);
		$(shadowsSelect, addOption, "Off", (ident) 0);

		$(theme, control, "Shadows", shadowsSelect);
		release(shadowsSelect);

		$(theme, checkbox, "Caustics", "r_caustics");
		$(theme, checkbox, "Weather effects", "cg_add_weather");

		$(theme, checkbox, "Bump mapping", "r_bumpmap");
		$(theme, checkbox, "Parallax mapping", "r_parallax");
		$(theme, checkbox, "Deluxe mapping", "r_deluxemap");
		$(theme, checkbox, "Stainmaps", "r_stainmaps");
		
		release(box);
	}

	$(theme, target, container);

	StackView *accessories = $(theme, accessories);

	$(theme, attach, accessories);
	$(theme, target, accessories);

	$(theme, button, "Apply", applyAction, self, NULL);

	release(accessories);
	release(columns);
	release(container);
	release(theme);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *VideoViewController::_VideoViewController(void)
 * @memberof VideoViewController
 */
Class *_VideoViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "VideoViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(VideoViewController);
		clazz.interfaceOffset = offsetof(VideoViewController, interface);
		clazz.interfaceSize = sizeof(VideoViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
