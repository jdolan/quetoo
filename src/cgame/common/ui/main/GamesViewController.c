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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cg_local.h"

#include "GamesViewController.h"

#define _Class _GamesViewController

#pragma mark - Games

/**
 * @return True if `game` is safe to interpolate into the `game` command.
 */
static bool isCommandSafe(const char *game) {

  for (const char *c = game; *c; c++) {
    if (!SDL_isalnum(*c) && *c != '_' && *c != '-' && *c != '.') {
      return false;
    }
  }

  return *game != '\0';
}

/**
 * @brief `ComGameEnumerator` that appends each game to the GamesViewController's games.
 */
static void enumerateGames(const char *game, const char *dir, void *data) {

  GamesViewController *this = data;

  if (!isCommandSafe(game)) {
    Cg_Warn("Skipping game %s: its name is not safe for the game command\n", game);
    return;
  }

  GameItemInfo *games = realloc(this->games, (this->numGames + 1) * sizeof(GameItemInfo));
  assert(games);

  this->games = games;

  GameItemInfo *info = &this->games[this->numGames++];

  Str_Copy(info->game, game, sizeof(info->game));
  Str_Copy(info->dir, dir, sizeof(info->dir));

  Cg_LoadGameInfo(&info->info, game, dir);
}

/**
 * @brief Sorts `default` first, then by title.
 */
static int32_t compareGames(const void *a, const void *b) {

  const GameItemInfo *x = a, *y = b;

  const bool xDefault = !Str_Compare(x->game, DEFAULT_GAME);
  const bool yDefault = !Str_Compare(y->game, DEFAULT_GAME);

  if (xDefault != yDefault) {
    return xDefault ? -1 : 1;
  }

  return SDL_strcasecmp(x->info.name, y->info.name);
}

/**
 * @brief Reads the installed games again, and selects the current game's card.
 */
static void reloadGames(GamesViewController *self) {

  free(self->games);
  self->games = NULL;
  self->numGames = 0;

  cgi.EnumerateGames(enumerateGames, self);

  qsort(self->games, self->numGames, sizeof(GameItemInfo), compareGames);

  $(self->gamesList, reloadData);

  for (size_t i = 0; i < self->numGames; i++) {
    if (!Str_Compare(self->games[i].game, cgi.Game())) {
      IndexPath *indexPath = $(alloc(IndexPath), initWithIndex, i);
      $(self->gamesList, selectItemAtIndexPath, indexPath);
      release(indexPath);
      break;
    }
  }
}

#pragma mark - CollectionViewDataSource

/**
 * @see CollectionViewDataSource::numberOfItems(const CollectionView *)
 */
static size_t numberOfItems(const CollectionView *collectionView) {

  const GamesViewController *this = collectionView->dataSource.self;

  return this->numGames;
}

/**
 * @see CollectionViewDataSource::objectForItemAtIndexPath(const CollectionView *, const IndexPath *)
 */
static ident objectForItemAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const GamesViewController *this = collectionView->dataSource.self;

  return (ident) &this->games[$(indexPath, indexAtPosition, 0)];
}

#pragma mark - CollectionViewDelegate

/**
 * @see CollectionViewDelegate::itemForObjectAtIndexPath(const CollectionView *, const IndexPath *)
 */
static CollectionItemView *itemForObjectAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const GamesViewController *this = collectionView->delegate.self;

  GameCollectionItemView *item = $(alloc(GameCollectionItemView), initWithFrame, NULL);
  assert(item);

  const size_t index = $(indexPath, indexAtPosition, 0);

  $(item, setGameItemInfo, &this->games[index]);

  return (CollectionItemView *) item;
}

/**
 * @brief Makes the selected game current. The `game` command reloads the client game, which
 * replaces this menu, so it runs from the command buffer, after this call returns.
 */
static void didModifySelection(CollectionView *collectionView, const Array *selectionIndexPaths) {

  const GamesViewController *this = collectionView->delegate.self;

  if (selectionIndexPaths->count == 0 || *cgi.state > CL_DISCONNECTED) {
    return;
  }

  const IndexPath *indexPath = $(selectionIndexPaths, objectAtIndex, 0);
  const GameItemInfo *info = &this->games[$(indexPath, indexAtPosition, 0)];

  if (Str_Compare(info->game, cgi.Game())) {
    cgi.Cbuf(va("game %s\n", info->game));
  }
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  GamesViewController *this = (GamesViewController *) self;

  free(this->games);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  GamesViewController *this = (GamesViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("gamesList", &this->gamesList)
  );

  $(self->view, awakeWithResourceName, "ui/main/GamesViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/main/GamesViewController.css");
  assert(self->view->stylesheet);

  this->gamesList->dataSource.self = this;
  this->gamesList->dataSource.numberOfItems = numberOfItems;
  this->gamesList->dataSource.objectForItemAtIndexPath = objectForItemAtIndexPath;

  this->gamesList->delegate.self = this;
  this->gamesList->delegate.itemForObjectAtIndexPath = itemForObjectAtIndexPath;
  this->gamesList->delegate.didModifySelection = didModifySelection;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  GamesViewController *this = (GamesViewController *) self;

  reloadGames(this);

  Control *gamesList = (Control *) this->gamesList;

  const ControlState state = *cgi.state > CL_DISCONNECTED
    ? gamesList->state | ControlStateDisabled
    : gamesList->state & ~ControlStateDisabled;

  if (state != gamesList->state) {
    gamesList->state = state;
    $(gamesList, stateDidChange);
  }

  super(ViewController, self, viewWillAppear);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
}

/**
 * @fn Class *GamesViewController::_GamesViewController(void)
 * @memberof GamesViewController
 */
Class *_GamesViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "GamesViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(GamesViewController),
      .interfaceSize = sizeof(GamesViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
