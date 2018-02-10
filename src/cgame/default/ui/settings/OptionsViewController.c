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

#include "CvarSelect.h"
#include "CvarSlider.h"

#include "QuetooTheme.h"

#define _Class _OptionsViewController

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

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Options");

	OptionsViewController *this = (OptionsViewController *) self;

	QuetooTheme *theme = $(alloc(QuetooTheme), initWithTarget, self->view);

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 3);

	$(theme, attach, columns);
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

	$(theme, targetSubview, columns, 2);

	{
		Box *box = $(theme, box, "Quality");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		Select *qualitySelect = $(alloc(Select), initWithFrame, NULL);

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

	 $(theme, targetSubview, columns, 2);

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

	release(columns);
	release(container);
	release(theme);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *OptionsViewController::_OptionsViewController(void)
 * @memberof OptionsViewController
 */
Class *_OptionsViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "OptionsViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(OptionsViewController),
			.interfaceOffset = offsetof(OptionsViewController, interface),
			.interfaceSize = sizeof(OptionsViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
