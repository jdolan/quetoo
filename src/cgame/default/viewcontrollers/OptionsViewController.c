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
#include "HueSlider.h"

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
 * @brief ActionFunction for crosshair color selection.
 */
static void didSelectCrosshairColor(ColorPicker *select) {

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

	StackView *stackView = $(alloc(StackView), initWithFrame, NULL);
	stackView->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *columns = $(alloc(StackView), initWithFrame, NULL);
		columns->spacing = DEFAULT_PANEL_SPACING;

		columns->axis = StackViewAxisHorizontal;
		columns->distribution = StackViewDistributionFillEqually;

		{
			StackView *column = $(alloc(StackView), initWithFrame, NULL);
			column->spacing = DEFAULT_PANEL_SPACING;

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Crosshair");

				Select *crosshairSelect = (Select *) $(alloc(CvarSelect), initWithVariable, cg_draw_crosshair);

				$((Select *) crosshairSelect, addOption, "", NULL);
				cgi.EnumerateFiles("pics/ch*", enumerateCrosshairs, crosshairSelect);

				Cgui_Input((View *) box->contentView, "Crosshair", (Control *) crosshairSelect);

				//Cgui_Input((View *) box->contentView, "Color", (Control *) crosshairColor);

				CvarSlider *crosshairScale = $(alloc(CvarSlider), initWithVariable, cg_draw_crosshair_scale, 0.1, 2.0, 0.1);

				Cgui_Input((View *) box->contentView, "Scale", (Control *) crosshairScale);

				Cgui_CvarCheckboxInput((View *) box->contentView, "Pulse on pickup", cg_draw_crosshair_pulse->name);

				// Crosshair preview

				const SDL_Rect frame = MakeRect(0, 0, 72, 72);

				CrosshairView *crosshairView = $(alloc(CrosshairView), initWithFrame, &frame);
				crosshairView->view.alignment = ViewAlignmentMiddleCenter;

				$((Control *) crosshairSelect, addActionForEventType, SDL_MOUSEBUTTONUP, updateCrosshair, NULL, crosshairView);
				$((Control *) crosshairScale, addActionForEventType, SDL_MOUSEMOTION, updateCrosshair, self, NULL);

				$((View *) crosshairView, updateBindings);

				$((View *) box->contentView, addSubview, (View *) crosshairView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "HUD");

				// Stats

				Cgui_CvarCheckboxInput((View *) box->contentView, "Show stats", "cl_draw_counters");
				Cgui_CvarCheckboxInput((View *) box->contentView, "Show netgraph", "cl_draw_net_graph");

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
				$(box->label, setText, "View");

				// Field of view

				Cgui_CvarSliderInput((View *) box->contentView, "FOV", cg_fov->name, 80.0, 130.0, 5.0);
				Cgui_CvarSliderInput((View *) box->contentView, "Zoom FOV", cg_fov_zoom->name, 20.0, 70.0, 5.0);

				// Zoom easing speed

				Cgui_CvarSliderInput((View *) box->contentView, "Zoom interpolate", cg_fov_interpolate->name, 0.0, 2.0, 0.1);

				// Bobbing

				Cgui_CvarCheckboxInput((View *) box->contentView, "View bobbing", cg_bob->name);
				Cgui_CvarCheckboxInput((View *) box->contentView, "Weapon swaying", cg_draw_weapon_bob->name);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Screen");

				// Blending

				Cgui_CvarCheckboxInput((View *) box->contentView, "Screen blending", cg_draw_blend->name);
				Cgui_CvarCheckboxInput((View *) box->contentView, "Liquid blend", cg_draw_blend_liquid->name);
				Cgui_CvarCheckboxInput((View *) box->contentView, "Pickup blend", cg_draw_blend_pickup->name);
				Cgui_CvarCheckboxInput((View *) box->contentView, "Powerup blend", cg_draw_blend_powerup->name);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Weapon");

				// Weapon options

				Cgui_CvarCheckboxInput((View *) box->contentView, "Draw weapon", cg_draw_weapon->name);
				Cgui_CvarCheckboxInput((View *) box->contentView, "Weapon bobbing", cg_bob->name);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}
			
			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		$((View *) stackView, addSubview, (View *) columns);
		release(columns);
	}
	
	$(self->view, addSubview, (View *) stackView);
	release(stackView);
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
