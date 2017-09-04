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
static void modifyCrosshair(Control *control, const SDL_Event *event, ident sender, ident data) {

	MouseViewController *this = (MouseViewController *) sender;

	$((View *) this->crosshairView, updateBindings);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MouseViewController *this = (MouseViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label->text, setText, "MOUSE");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cg_CvarSliderInput((View *) stackView, "Sensitivity", "m_sensitivity", 0.1, 6.0, 0.1);
			Cg_CvarSliderInput((View *) stackView, "Zoom Sensitivity", "m_sensitivity_zoom", 0.1, 6.0, 0.1);
			Cg_CvarCheckboxInput((View *) stackView, "Invert mouse", "m_invert");
			Cg_CvarCheckboxInput((View *) stackView, "Smooth mouse", "m_interpolate");

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label->text, setText, "CROSSHAIR");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			CvarSelect *crosshairSelect = $(alloc(CvarSelect), initWithVariable, cg_draw_crosshair);
			$((Select *) crosshairSelect, addOption, "", NULL);
			cgi.EnumerateFiles("pics/ch*", enumerateCrosshairs, crosshairSelect);

			$((Control *) crosshairSelect, addActionForEventType, SDL_MOUSEBUTTONUP, modifyCrosshair, self, NULL);
			Cg_Input((View *) stackView, "Crosshair", (Control *) crosshairSelect);

			CvarSelect *colorSelect = (CvarSelect *) $(alloc(CvarSelect), initWithVariable, cg_draw_crosshair_color);
			colorSelect->expectsStringValue = true;

			$((Select *) colorSelect, addOption, "default", (ident) 0);
			$((Select *) colorSelect, addOption, "red", (ident) 1);
			$((Select *) colorSelect, addOption, "green", (ident) 2);
			$((Select *) colorSelect, addOption, "yellow", (ident) 3);
			$((Select *) colorSelect, addOption, "orange", (ident) 4);

			$((Control *) colorSelect, addActionForEventType, SDL_MOUSEBUTTONUP, modifyCrosshair, self, NULL);
			Cg_Input((View *) stackView, "Crosshair color", (Control *) colorSelect);

			CvarSlider *scaleSlider = $(alloc(CvarSlider), initWithVariable, cg_draw_crosshair_scale, 0.1, 2.0, 0.1);

			$((Control *) scaleSlider, addActionForEventType, SDL_MOUSEMOTION, modifyCrosshair, self, NULL);
			Cg_Input((View *) stackView, "Crosshair scale", (Control *) scaleSlider);

			Cg_CvarCheckboxInput((View *) stackView, "Pulse on pickup", cg_draw_crosshair_pulse->name);

			const SDL_Rect frame = MakeRect(0, 0, 72, 72);

			this->crosshairView = $(alloc(CrosshairView), initWithFrame, &frame);
			this->crosshairView->view.alignment = ViewAlignmentMiddleCenter;

			$((View *) stackView, addSubview, (View *) this->crosshairView);
			release(this->crosshairView);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((View *) this->menuViewController.panel->contentView, addSubview, (View *) columns);
	release(columns);
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
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(MouseViewController);
		clazz.interfaceOffset = offsetof(MouseViewController, interface);
		clazz.interfaceSize = sizeof(MouseViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class

