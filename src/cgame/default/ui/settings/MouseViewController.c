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

#include "MouseViewController.h"
#include "CrosshairView.h"
#include "CvarSelect.h"
#include "CvarSlider.h"

#include "Theme.h"

#define _Class _MouseViewController

#pragma mark - Crosshair selection

/**
 * @brief Fs_EnumerateFunc for crosshair selection.
 */
static void enumerateCrosshairs(const char *path, void *data) {
	char name[MAX_QPATH];

	StripExtension(Basename(path), name);

	intptr_t value = strtol(name + strlen("ch"), NULL, 10);
	assert(value);

	$((Select *) data, addOption, name, (ident) value);
}

/**
 * @brief ActionFunction for crosshair modification.
 */
static void crosshairAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	$((View *) data, updateBindings);
}

/**
 * @brief HueColorPickerDelegate callback for crosshair color selection.
 */
static void didPickColor(HueColorPicker *colorPicker, double hue, double saturation, double value) {

}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Mouse");

	MouseViewController *this = (MouseViewController *) self;

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 2);
	$(theme, attach, columns);

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Response");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, slider, "Sensitivity", "m_sensitivity", 0.1, 6.0, 0.1);
		$(theme, slider, "Zoom Sensitivity", "m_sensitivity_zoom", 0.1, 6.0, 0.1);
		$(theme, checkbox, "Invert mouse", "m_invert");
		$(theme, checkbox, "Smooth mouse", "m_interpolate");

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Crosshair");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		Select *crosshair = (Select *) $(alloc(CvarSelect), initWithVariable, cg_draw_crosshair);

		$(crosshair, addOption, "", NULL);
		cgi.EnumerateFiles("pics/ch*", enumerateCrosshairs, crosshair);

		$(theme, control, "Crosshair", crosshair);

		HueColorPicker *crosshairColor = $(alloc(HueColorPicker), initWithFrame, NULL, ControlStyleDefault);
		assert(crosshairColor);

		crosshairColor->delegate.self = this;
		crosshairColor->delegate.didPickColor = didPickColor;

		$(theme, control, "Color", crosshairColor);

		CvarSlider *crosshairScale = $(alloc(CvarSlider), initWithVariable, cg_draw_crosshair_scale, 0.1, 2.0, 0.1);

		$(theme, control, "Scale", crosshairScale);
		$(theme, checkbox, "Pulse on Pickup", cg_draw_crosshair_pulse->name);

		CrosshairView *crosshairView = $(alloc(CrosshairView), initWithFrame, &MakeRect(0, 0, 72, 72));
		assert(crosshair);

		crosshairView->view.alignment = ViewAlignmentMiddleCenter;

		$((Control *) crosshair, addActionForEventType, SDL_MOUSEBUTTONUP, crosshairAction, self, crosshairView);
		$((Control *) crosshairScale, addActionForEventType, SDL_MOUSEMOTION, crosshairAction, self, crosshairView);

		$((View *) crosshairView, updateBindings);

		$(theme, attach, crosshairView);
		release(crosshairView);
		
		release(box);
	}

	release(columns);
	release(container);
	release(theme);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *MouseViewController::_MouseViewController(void)
 * @memberof MouseViewController
 */
Class *_MouseViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "MouseViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(MouseViewController);
		clazz.interfaceOffset = offsetof(MouseViewController, interface);
		clazz.interfaceSize = sizeof(MouseViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class

