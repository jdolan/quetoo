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

#pragma mark CollectionViewDataSource

/**
 * @see CollectionViewDataSource::numberOfItems(const CollectionView *)
 */
static size_t numberOfItems(const CollectionView *collectionView) {
	return cg_state.num_teams ?: 1;
}

/**
 * @see CollectionViewDataSource::objectForItemAtIndexPath(const CollectionView *, const IndexPath *)
 */
static ident objectForItemAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

	if (cg_state.num_teams == 0) {
		return NULL;
	}

	const size_t index = $(indexPath, indexAtPosition, 0);
	return &cg_state.teams[index];
}

#pragma mark - CollectionViewDelegate

/**
 * @see CollectionViewDelegate::itemForObjectAtIndex(const CollectionView *, const IndexPath *)
 */
static CollectionItemView *itemForObjectAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

	const cg_team_info_t *team = objectForItemAtIndexPath(collectionView, indexPath);

	TeamView *teamView = $(alloc(TeamView), initWithFrame, NULL);
	$(teamView, setTeam, team);

	CollectionItemView *item = $(alloc(CollectionItemView), initWithFrame, NULL);
	$((View *) item, addSubview, (View *) teamView);

	return item;
}

#pragma mark - Actions

/**
 * @brief ActionFunction for spectate Button.
 */
static void spectateAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	cgi.Cbuf("spectate\n");

	cgi.SetKeyDest(KEY_GAME);
}

/**
 * @brief ActionFunction for join Button.
 */
static void joinAction(Control *self, const SDL_Event *event, ident sender, ident data) {

	TeamsViewController *this = sender;

	Array *paths = $(this->teamsCollectionView, selectionIndexPaths);
	IndexPath *path = $(paths, firstObject);

	if (path) {
		const cg_team_info_t *team = objectForItemAtIndexPath(this->teamsCollectionView, path);
		if (team) {
			cgi.Cbuf(va("team %s\n", team->name));
		} else {
			cgi.Cbuf(va("join\n"));
		}

		cgi.SetKeyDest(KEY_GAME);
	}

	release(path);
	release(paths);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	TeamsViewController *this = (TeamsViewController *) self;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("teamsCollectionView", &this->teamsCollectionView),
		MakeOutlet("spectate", &this->spectate),
		MakeOutlet("join", &this->join)
	);

	View *view = $$(View, viewWithResourceName, "ui/teams/TeamsViewController.json", outlets);
	assert(view);

	$((Control *) this->spectate, addActionForEventType, SDL_MOUSEBUTTONUP, spectateAction, self, NULL);
	$((Control *) this->join, addActionForEventType, SDL_MOUSEBUTTONUP, joinAction, self, NULL);

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

	IndexPath *index = $(alloc(IndexPath), initWithIndex, 0);
	$(this->teamsCollectionView, selectItemAtIndexPath, index);
	release(index);
}

/**
 * @see ViewController::viewDidAppear(ViewController *)
 */
static void viewDidAppear(ViewController *self) {

	super(ViewController, self, viewWillAppear);

	TeamsViewController *this = (TeamsViewController *) self;

	$((View *) this->teamsCollectionView, sizeToFit);

	$(self->view, sizeToFit);
}

#pragma mark - TeamsViewController

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
	((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
	((ViewControllerInterface *) clazz->interface)->viewDidAppear = viewDidAppear;
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
