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
#include "VideoModeSelect.h"

#define _Class _VideoViewController

#pragma mark - Actions and delegate callbacks

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

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Video");

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
				$(box->label, setText, "Display");

				Control *videoModeSelect = (Control *) $(alloc(VideoModeSelect), initWithFrame, NULL, ControlStyleDefault);

				Cgui_Input((View *) box->contentView, "Video mode", videoModeSelect);
				release(videoModeSelect);

				cvar_t *r_fullscreen = cgi.CvarGet("r_fullscreen");
				Select *fullscreenSelect = (Select *) $(alloc(CvarSelect), initWithVariable, r_fullscreen);

				$(fullscreenSelect, addOption, "Windowed", (ident) 0);
				$(fullscreenSelect, addOption, "Fullscreen", (ident) 1);
				$(fullscreenSelect, addOption, "Borderless windowed", (ident) 2);

				Cgui_Input((View *) box->contentView, "Window mode", (Control *) fullscreenSelect);
				release(fullscreenSelect);

				cvar_t *r_swap_interval = cgi.CvarGet("r_swap_interval");
				Select *swapIntervalSelect = (Select *) $(alloc(CvarSelect), initWithVariable, r_swap_interval);

				$(swapIntervalSelect, addOption, "On", (ident) 1);
				$(swapIntervalSelect, addOption, "Off", (ident) 0);
				$(swapIntervalSelect, addOption, "Adaptive", (ident) -1);

				Cgui_Input((View *) box->contentView, "Vertical sync", (Control *) swapIntervalSelect);
				release(swapIntervalSelect);

				Cgui_CvarSliderInput((View *) box->contentView, "Maximum FPS", "cl_max_fps", 0.0, 200.0, 5.0);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Rendering");

				Select *anisoSelect = (Select *) $(alloc(CvarSelect), initWithVariableName, "r_anisotropy");

				$(anisoSelect, addOption, "16x", (ident) 16);
				$(anisoSelect, addOption, "8x", (ident) 8);
				$(anisoSelect, addOption, "4x", (ident) 4);
				$(anisoSelect, addOption, "2x", (ident) 2);
				$(anisoSelect, addOption, "Off", (ident) 0);

				Cgui_Input((View *) box->contentView, "Anisotropy", (Control *) anisoSelect);
				release(anisoSelect);

				Select *multisampleSelect = (Select *) $(alloc(CvarSelect), initWithVariableName, "r_multisample");

				$(multisampleSelect, addOption, "8x", (ident) 4);
				$(multisampleSelect, addOption, "4x", (ident) 2);
				$(multisampleSelect, addOption, "2x", (ident) 1);
				$(multisampleSelect, addOption, "Off", (ident) 0);

				Cgui_Input((View *) box->contentView, "Multisample", (Control *) multisampleSelect);
				release(multisampleSelect);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Picture");

				Cgui_CvarSliderInput((View *) box->contentView, "Brightness", "r_brightness", 0.1, 2.0, 0.1);
				Cgui_CvarSliderInput((View *) box->contentView, "Contrast", "r_contrast", 0.1, 2.0, 0.1);
				Cgui_CvarSliderInput((View *) box->contentView, "Gamma", "r_gamma", 0.1, 2.0, 0.1);
				Cgui_CvarSliderInput((View *) box->contentView, "Modulate", "r_modulate", 0.1, 5.0, 0.1);

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
				$(box->label, setText, "Quality");

				Select *qualitySelect = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

				$(qualitySelect, addOption, "Highest", (ident) 3);
				$(qualitySelect, addOption, "High", (ident) 2);
				$(qualitySelect, addOption, "Medium", (ident) 1);
				$(qualitySelect, addOption, "Low", (ident) 0);

				qualitySelect->delegate.didSelectOption = didSelectQuality;

				Cgui_Input((View *) box->contentView, "Quality", (Control *) qualitySelect);
				release(qualitySelect);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Effects");

				cvar_t *r_shadows = cgi.CvarGet("r_shadows");
				Select *shadowsSelect = (Select *) $(alloc(CvarSelect), initWithVariable, r_shadows);

				$(shadowsSelect, addOption, "Highest", (ident) 3);
				$(shadowsSelect, addOption, "High", (ident) 2);
				$(shadowsSelect, addOption, "Low", (ident) 1);
				$(shadowsSelect, addOption, "Off", (ident) 0);

				Cgui_Input((View *) box->contentView, "Shadows", (Control *) shadowsSelect);
				release(shadowsSelect);

				Cgui_CvarCheckboxInput((View *) box->contentView, "Caustics", "r_caustics");
				Cgui_CvarCheckboxInput((View *) box->contentView, "Weather effects", "cg_add_weather");

				Cgui_CvarCheckboxInput((View *) box->contentView, "Bump mapping", "r_bumpmap");
				Cgui_CvarCheckboxInput((View *) box->contentView, "Parallax mapping", "r_parallax");
				Cgui_CvarCheckboxInput((View *) box->contentView, "Deluxe mapping", "r_deluxemap");

				Cgui_CvarCheckboxInput((View *) box->contentView, "Stainmaps", "r_stainmaps");
				
				$((View *) column, addSubview, (View *) box);
				release(box);
			}
			
			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		$((View *) stackView, addSubview, (View *) columns);
		release(columns);
	}

	{
		StackView *accessories = $(alloc(StackView), initWithFrame, NULL);

		accessories->axis = StackViewAxisHorizontal;
		accessories->spacing = DEFAULT_PANEL_SPACING;
		accessories->view.alignment = ViewAlignmentBottomRight;

		Cgui_Button((View *) accessories, "Apply", applyAction, self, NULL);

		$((View *) stackView, addSubview, (View *) accessories);
		release(accessories);
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
