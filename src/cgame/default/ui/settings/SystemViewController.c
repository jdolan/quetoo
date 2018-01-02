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

#include "SystemViewController.h"

#include "CvarSelect.h"
#include "QuetooTheme.h"

#define _Class _SystemViewController

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

	SystemViewController *this = (SystemViewController *) self;

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
	self->view->identifier = strdup("System");

	SystemViewController *this = (SystemViewController *) self;

	QuetooTheme *theme = $(alloc(QuetooTheme), initWithTarget, self->view);

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

		this->videoModeSelect = $(alloc(VideoModeSelect), initWithFrame, NULL);
		assert(this->videoModeSelect);

		this->videoModeSelect->select.delegate.self = this;
		this->videoModeSelect->select.delegate.didSelectOption = didSelecVideoMode;

		$(theme, control, "Video mode", this->videoModeSelect);

		$(theme, checkbox, "High DPI (4K)", "r_allow_high_dpi");

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

		$(theme, slider, "Framerate limiter", "cl_max_fps", 0.0, 240.0, 5.0, NULL);

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

		$(theme, slider, "Brightness", "r_brightness", 0.1, 2.0, 0.1, NULL);
		$(theme, slider, "Contrast", "r_contrast", 0.1, 2.0, 0.1, NULL);
		$(theme, slider, "Gamma", "r_gamma", 0.1, 2.0, 0.1, NULL);
		$(theme, slider, "Modulate", "r_modulate", 0.1, 5.0, 0.1, NULL);

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Sound");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, slider, "Master", "s_volume", 0.0, 1.0, 0.1, NULL);
		$(theme, slider, "Effects", "s_effects_volume", 0.0, 1.0, 0.1, NULL);
		$(theme, slider, "Ambient", "s_ambient_volume", 0.0, 1.0, 0.1, NULL);
		$(theme, slider, "Music", "s_music_volume", 0.0, 1.0, 0.1, NULL);

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Network");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		CvarSelect *rate = $(alloc(CvarSelect), initWithVariableName, "rate");

		$((Select *) rate, addOption, "100Mbps", (ident) (intptr_t) 0);
		$((Select *) rate, addOption, "50Mbps", (ident) (intptr_t) 50000);
		$((Select *) rate, addOption, "20Mbps", (ident) (intptr_t) 20000);
		$((Select *) rate, addOption, "10Mbps", (ident) (intptr_t) 10000);

		$(theme, control, "Connection speed", rate);

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
 * @fn Class *SystemViewController::_SystemViewController(void)
 * @memberof SystemViewController
 */
Class *_SystemViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "SystemViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(SystemViewController);
		clazz.interfaceOffset = offsetof(SystemViewController, interface);
		clazz.interfaceSize = sizeof(SystemViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
