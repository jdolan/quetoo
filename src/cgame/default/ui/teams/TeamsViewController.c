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

#include "TeamsViewController.h"
#include "TeamView.h"

#define _Class _TeamsViewController

#pragma mark - Actions

#pragma mark CollectionViewDataSource

/**
 * @see CollectionViewDataSource::numberOfItems(const CollectionView *)
 */
static size_t numberOfItems(const CollectionView *collectionView) {
	return cg_state.num_teams;
}

/**
 * @see CollectionViewDataSource::objectForItemAtIndexPath(const CollectionView *, const IndexPath *)
 */
static ident objectForItemAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

	const size_t index = $(indexPath, indexAtPosition, 0);

	return &cg_state.teams[index];
}

#pragma mark - CollectionViewDelegate

/**
 * @see CollectionViewDelegate::itemForObjectAtIndex(const CollectionView *, const IndexPath *)
 */
static CollectionItemView *itemForObjectAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

	TeamView *teamView = $(alloc(TeamView), initWithFrame, NULL);

	const size_t index = $(indexPath, indexAtPosition, 0);
	$(teamView, setTeam, &cg_state.teams[index]);

	CollectionItemView *item = $(alloc(CollectionItemView), initWithFrame, NULL);
	$((View *) item, addSubview, (View *) teamView);

	return item;
}
#pragma mark - ViewController

/**
 * @see ViewController::handleNotification(ViewController *, const Notification *)
 */
static void handleNotification(ViewController *self, const Notification *notification) {

	super(ViewController, self, handleNotification, notification);
}

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	TeamsViewController *this = (TeamsViewController *) self;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("teamsCollectionView", &this->teamsCollectionView)
	);

	View *view = $$(View, viewWithResourceName, "ui/teams/TeamsViewController.json", outlets);
	assert(view);

	$(self, setView, view);
	release(view);

	this->teamsCollectionView->dataSource.numberOfItems = numberOfItems;
	this->teamsCollectionView->dataSource.objectForItemAtIndexPath = objectForItemAtIndexPath;

	this->teamsCollectionView->delegate.itemForObjectAtIndexPath = itemForObjectAtIndexPath;

	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/teams/TeamsViewController.css");
	assert(self->view->stylesheet);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	super(ViewController, self, viewWillAppear);

	TeamsViewController *this = (TeamsViewController *) self;

	$(this->teamsCollectionView, reloadData);

	$(self->view, resize, &MakeSize(0, 0));

	$((View *) this->teamsCollectionView, sizeToFit);

	$(self->parentViewController->view, layoutSubviews);
}

#pragma mark - TeamsViewController

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->handleNotification = handleNotification;
	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
	((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
}

/**
 * @fn Class *TeamsViewController::_TeamsViewController(void)
 * @memberof TeamsViewController
 */
Class *_TeamsViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "TeamsViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(TeamsViewController),
			.interfaceOffset = offsetof(TeamsViewController, interface),
			.interfaceSize = sizeof(TeamsViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
