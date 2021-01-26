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

#define _Class _OptionsViewController

#pragma mark - Actions and delegate callbacks

/**
 * @brief SelectDelegate callback for Quality.
 */
static void didSelectQuality(Select *select, Option *option) {

	switch ((intptr_t) option->value) {
		case 3:
			cgi.SetCvarInteger("cg_add_atmospheric", 1);
			cgi.SetCvarInteger("cg_add_entity_shadows", 3);
			cgi.SetCvarInteger("cg_add_weather", 1);
			cgi.SetCvarInteger("r_caustics", 1);
			cgi.SetCvarInteger("r_sprites_soften", 1);
			cgi.SetCvarInteger("r_sprites_lerp", 1);
			cgi.SetCvarInteger("r_stains", 1);
			break;
		case 2:
			cgi.SetCvarInteger("cg_add_atmospheric", 1);
			cgi.SetCvarInteger("cg_add_entity_shadows", 2);
			cgi.SetCvarInteger("cg_add_weather", 1);
			cgi.SetCvarInteger("r_caustics", 1);
			cgi.SetCvarInteger("r_sprites_soften", 1);
			cgi.SetCvarInteger("r_sprites_lerp", 1);
			cgi.SetCvarInteger("r_stains", 1);
			break;
		case 1:
			cgi.SetCvarInteger("cg_add_atmospheric", 1);
			cgi.SetCvarInteger("cg_add_entity_shadows", 1);
			cgi.SetCvarInteger("cg_add_weather", 1);
			cgi.SetCvarInteger("r_caustics", 0);
			cgi.SetCvarInteger("r_sprites_soften", 1);
			cgi.SetCvarInteger("r_sprites_lerp", 0);
			cgi.SetCvarInteger("r_stains", 1);
			break;
		case 0:
			cgi.SetCvarInteger("cg_add_atmospheric", 0);
			cgi.SetCvarInteger("cg_add_entity_shadows", 0);
			cgi.SetCvarInteger("cg_add_weather", 0);
			cgi.SetCvarInteger("r_caustics", 0);
			cgi.SetCvarInteger("r_sprites_soften", 0);
			cgi.SetCvarInteger("r_sprites_lerp", 0);
			
			break;
		default:
			break;
	}

	ViewController *this = select->delegate.self;
	if (this) {
		$(this->view, updateBindings);
	}
}

/**
 * @brief SelectDelegate callback for sprites quality.
 */
static void didSelectSprites(Select *select, Option *option) {

	switch ((intptr_t) option->value) {
		case 0:
			cgi.SetCvarInteger("r_sprites_soften", 0);
			cgi.SetCvarInteger("r_sprites_lerp", 0);
			break;
		case 1:
			cgi.SetCvarInteger("r_sprites_soften", 1);
			cgi.SetCvarInteger("r_sprites_lerp", 0);
			break;
		case 2:
			cgi.SetCvarInteger("r_sprites_soften", 1);
			cgi.SetCvarInteger("r_sprites_lerp", 1);
			break;
		default:
			break;
	}

	ViewController *this = select->delegate.self;
	if (this) {
		$(this->view, updateBindings);
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	Select *quality, *weather, *fog, *atmospheric, *shadows, *sprites;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("quality", &quality),
		MakeOutlet("weather", &weather),
		MakeOutlet("fog", &fog),
		MakeOutlet("atmospheric", &atmospheric),
		MakeOutlet("shadows", &shadows),
		MakeOutlet("sprites", &sprites)
	);

	$(self->view, awakeWithResourceName, "ui/settings/OptionsViewController.json");
	$(self->view, resolve, outlets);

	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/settings/OptionsViewController.css");
	assert(self->view->stylesheet);

	$(quality, addOption, "Custom", (ident) -1);
	$(quality, addOption, "Highest", (ident) 3);
	$(quality, addOption, "High", (ident) 2);
	$(quality, addOption, "Medium", (ident) 1);
	$(quality, addOption, "Low", (ident) 0);

	quality->delegate.self = self;
	quality->delegate.didSelectOption = didSelectQuality;

	$(weather, addOption, "Heavy", (ident) 2);
	$(weather, addOption, "Normal", (ident) 1);
	$(weather, addOption, "Off", (ident) 0);

	$(fog, addOption, "High", (ident) 32);
	$(fog, addOption, "Medium", (ident) 16);
	$(fog, addOption, "Low", (ident) 8);

	$(atmospheric, addOption, "Heavy", (ident) 2);
	$(atmospheric, addOption, "Normal", (ident) 1);
	$(atmospheric, addOption, "Off", (ident) 8);

	$(shadows, addOption, "High", (ident) 3);
	$(shadows, addOption, "Medium", (ident) 2);
	$(shadows, addOption, "Low", (ident) 1);
	$(shadows, addOption, "Off", (ident) 0);

	$(sprites, addOption, "High", (ident) 2);
	$(sprites, addOption, "Medium", (ident) 1);
	$(sprites, addOption, "Low", (ident) 8);

	sprites->delegate.self = self;
	sprites->delegate.didSelectOption = didSelectSprites;
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
