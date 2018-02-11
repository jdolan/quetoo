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

#include "HomeViewController.h"

#define _Class _HomeViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	HomeViewController *this = (HomeViewController *) self;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("motd", &this->motd)
	);

	View *view = cgi.View("ui/home/HomeViewController.json", outlets);

	$(self, setView, view);

	release(view);

	$(this->motd->text, setText, "Message of the day");
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *HomeViewController::_HomeViewController(void)
 * @memberof HomeViewController
 */
Class *_HomeViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "HomeViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(HomeViewController),
			.interfaceOffset = offsetof(HomeViewController, interface),
			.interfaceSize = sizeof(HomeViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
