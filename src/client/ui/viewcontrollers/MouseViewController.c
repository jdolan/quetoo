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

#include <assert.h>

#include "MouseViewController.h"

#include "ui_data.h"

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

	StackView *columns = alloc(StackView, initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = alloc(StackView, initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = alloc(Box, initWithFrame, NULL);
			$(box->label, setText, "MOUSE");

			StackView *stackView = alloc(StackView, initWithFrame, NULL);

			extern cvar_t *m_sensitivity;
			extern cvar_t *m_sensitivity_zoom;
			extern cvar_t *m_invert;
			extern cvar_t *m_interpolate;

			Ui_CvarSlider((View *) stackView, "Sensitivity", m_sensitivity, 0.1, 6.0, 0.1);
			Ui_CvarSlider((View *) stackView, "Zoom Sensitivity", m_sensitivity_zoom, 0.1, 6.0, 0.1);
			Ui_CvarCheckbox((View *) stackView, "Invert mouse", m_invert);
			Ui_CvarCheckbox((View *) stackView, "Smooth mouse", m_interpolate);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	{
		StackView *column = alloc(StackView, initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = alloc(Box, initWithFrame, NULL);
			$(box->label, setText, "CROSSHAIR");

			StackView *stackView = alloc(StackView, initWithFrame, NULL);

			cvar_t *cg_draw_crosshair = Cvar_Get("cg_draw_crosshair", "1", 0, NULL);
			cvar_t *cg_draw_crosshair_color = Cvar_Get("cg_draw_crosshair_color", "", 0, NULL);
			cvar_t *cg_draw_crosshair_pulse = Cvar_Get("cg_draw_crosshair_pulse", "1", 0, NULL);
			cvar_t *cg_draw_crosshair_scale = Cvar_Get("cg_draw_crosshair_scale", "1.0", 0, NULL);

			CvarSelect *crosshairSelect = alloc(CvarSelect, initWithVariable, cg_draw_crosshair);
			$((Select *) crosshairSelect, addOption, "", NULL);
			Fs_Enumerate("pics/ch*", enumerateCrosshairs, crosshairSelect);

			$((Control *) crosshairSelect, addActionForEventType, SDL_MOUSEBUTTONUP, modifyCrosshair, self, NULL);
			Ui_Input((View *) stackView, "Crosshair", (Control *) crosshairSelect);

			CvarSelect *colorSelect = (CvarSelect *) alloc(CvarSelect, initWithVariable, cg_draw_crosshair_color);
			colorSelect->expectsStringValue = true;

			$((Select *) colorSelect, addOption, "default", (ident) 0);
			$((Select *) colorSelect, addOption, "red", (ident) 1);
			$((Select *) colorSelect, addOption, "green", (ident) 2);
			$((Select *) colorSelect, addOption, "yellow", (ident) 3);
			$((Select *) colorSelect, addOption, "orange", (ident) 4);

			$((Control *) colorSelect, addActionForEventType, SDL_MOUSEBUTTONUP, modifyCrosshair, self, NULL);
			Ui_Input((View *) stackView, "Crosshair color", (Control *) colorSelect);

			CvarSlider *scaleSlider = alloc(CvarSlider, initWithVariable, cg_draw_crosshair_scale, 0.1, 2.0, 0.1);

			$((Control *) scaleSlider, addActionForEventType, SDL_MOUSEMOTION, modifyCrosshair, self, NULL);
			Ui_Input((View *) stackView, "Crosshair scale", (Control *) scaleSlider);

			Ui_CvarCheckbox((View *) stackView, "Pulse on pickup", cg_draw_crosshair_pulse);

			const SDL_Rect frame = MakeRect(0, 0, 72, 72);

			this->crosshairView = alloc(CrosshairView, initWithFrame, &frame);
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
	
	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _MouseViewController = {
	.name = "MouseViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(MouseViewController),
	.interfaceOffset = offsetof(MouseViewController, interface),
	.interfaceSize = sizeof(MouseViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

