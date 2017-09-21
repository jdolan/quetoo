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

#include "OptionsViewController.h"

#include "CrosshairView.h"
#include "CvarSelect.h"
#include "CvarSlider.h"

#include "Theme.h"

#define _Class _OptionsViewController

#pragma mark - Actions and delegate callbacks

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
static void updateCrosshair(Control *control, const SDL_Event *event, ident sender, ident data) {
	$((View *) data, updateBindings);
}

/**
 * @brief HueColorPickerDelegate callback for crosshair color selection.
 */
static void didPickColor(HueColorPicker *colorPicker, double hue, double saturation, double value) {

//	color_t color = ColorFromRGBA(colorSelect->color.r, colorSelect->color.g, colorSelect->color.b, colorSelect->color.a);
//	char hexColor[COLOR_MAX_LENGTH];
//
//	ColorToHex(color, hexColor, sizeof(hexColor));
//
//	cgi.CvarSet(cg_draw_crosshair_color->name, hexColor);
//
//	if (this->crosshairView) {
//		$((View *) this->crosshairView, updateBindings);
//	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Options");

	OptionsViewController *this = (OptionsViewController *) self;

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 2);

	$(theme, attach, columns);
	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Crosshair");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		Select *crosshair = (Select *) $(alloc(CvarSelect), initWithVariable, cg_draw_crosshair);

		$(crosshair, addOption, "", NULL);
		cgi.EnumerateFiles("pics/ch*", enumerateCrosshairs, crosshair);

		$(theme, control, "Crosshair", crosshair);

		this->crosshairColorPicker = $(alloc(HueColorPicker), initWithFrame, NULL);
		assert(this->crosshairColorPicker);

		this->crosshairColorPicker->delegate.self = this;
		this->crosshairColorPicker->delegate.didPickColor = didPickColor;
		
		$(theme, control, "Color", this->crosshairColorPicker);

		CvarSlider *crosshairScale = $(alloc(CvarSlider), initWithVariable, cg_draw_crosshair_scale, 0.1, 2.0, 0.1);

		$(theme, control, "Scale", crosshairScale);
		$(theme, checkbox, "Pulse on Pickup", cg_draw_crosshair_pulse->name);

		const SDL_Rect frame = MakeRect(0, 0, 72, 72);
		CrosshairView *crosshairView = $(alloc(CrosshairView), initWithFrame, &frame);
		crosshairView->view.alignment = ViewAlignmentMiddleCenter;

		$((Control *) crosshair, addActionForEventType, SDL_MOUSEBUTTONUP, updateCrosshair, self, crosshairView);
		//$((Control *) crosshairColor, addActionForEventType, SDL_MOUSEBUTTONUP, updateCrosshair, self, crosshairView);
		$((Control *) crosshairScale, addActionForEventType, SDL_MOUSEMOTION, updateCrosshair, self, crosshairView);

		$((View *) crosshairView, updateBindings);

		$(theme, attach, crosshairView);
		release(crosshairView);

		release(box);
	}

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Performance");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, checkbox, "Counters", "cl_draw_counters");
		$(theme, checkbox, "Net graph", "cl_draw_net_graph");

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "View");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, slider, "FOV", cg_fov->name, 80.0, 130.0, 5.0);
		$(theme, slider, "Zoom FOV", cg_fov_zoom->name, 20.0, 70.0, 5.0);
		$(theme, slider, "Zoom speed", cg_fov_interpolate->name, 0.0, 2.0, 0.1);

		$(theme, checkbox, "Draw weapon", cg_draw_weapon->name);
		$(theme, checkbox, "Weapon sway", cg_draw_weapon_bob->name);
		$(theme, checkbox, "Bob view", cg_bob->name);

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Screen");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, checkbox, "Screen blending", cg_draw_blend->name);
		$(theme, checkbox, "Liquid blend", cg_draw_blend_liquid->name);
		$(theme, checkbox, "Pickup blend", cg_draw_blend_pickup->name);
		$(theme, checkbox, "Powerup blend", cg_draw_blend_powerup->name);

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
 * @fn Class *OptionsViewController::_OptionsViewController(void)
 * @memberof OptionsViewController
 */
Class *_OptionsViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "OptionsViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(OptionsViewController);
		clazz.interfaceOffset = offsetof(OptionsViewController, interface);
		clazz.interfaceSize = sizeof(OptionsViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
