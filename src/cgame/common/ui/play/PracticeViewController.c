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

#include "PracticeViewController.h"

#define _Class _PracticeViewController

#pragma mark - Private

/**
 * @brief True if the ladder loaded again since the tiers were built, so that they point into a
 * ladder that is freed.
 */
static bool isStale(const PracticeViewController *self) {
  return Cg_Practice_LadderGeneration() != self->tiersGeneration;
}

/**
 * @brief The entry with `index` in the ladder, or `NULL`.
 */
static const CGamePracticeEntry *entryAtIndex(int32_t index) {

  size_t count;
  const CGamePracticeEntry *ladder = Cg_Practice_Ladder(&count);

  return index >= 0 && (size_t) index < count ? &ladder[index] : NULL;
}

/**
 * @brief The gameplay that the ladder sets, or else the selected gameplay, by name.
 * @remarks The chooser does not offer team gameplays, so it can not select a team ladder's.
 */
static const char *selectedGamePlay(const PracticeViewController *self) {

  const GamePlay *gamePlay = Cg_Practice_LadderGamePlay();
  if (gamePlay) {
    return gamePlay->name;
  }

  const Option *option = $(self->gameplay, selectedOption);
  return option ? option->value : "deathmatch";
}

/**
 * @brief Enables or disables `control`.
 */
static void setEnabled(Control *control, bool enabled) {

  const ControlState state = enabled
    ? control->state & ~ControlStateDisabled
    : control->state | ControlStateDisabled;

  if (state != control->state) {
    control->state = state;
    $(control, stateDidChange);
  }
}

/**
 * @brief Selects the gameplay that the ladder sets, or else deathmatch, and lets the player
 * choose only if the ladder sets none.
 */
static void updateGamePlay(PracticeViewController *self) {

  const GamePlay *gamePlay = Cg_Practice_LadderGamePlay();
  const char *name = gamePlay ? gamePlay->name : "deathmatch";

  size_t count;
  const GamePlay *gamePlays = Cg_ListGamePlays(&count);

  for (size_t i = 0; i < count; i++) {
    if (!Str_Compare(gamePlays[i].name, name)) {
      $(self->gameplay, selectOptionWithValue, (ident) gamePlays[i].name);
      break;
    }
  }

  setEnabled((Control *) self->gameplay, gamePlay == NULL);
}

/**
 * @brief Shows the selected entry, and enables the Play button only if an entry is selected.
 */
static void updateDetails(PracticeViewController *self) {

  const CGamePracticeEntry *entry = entryAtIndex(self->selected);

  if (entry) {
    $(self->map, setText, self->maps[self->selected].message);
    $(self->opponents, setNames, entry->ai);

    char limits[MAX_STRING_CHARS] = "";
    if (entry->fragLimit > -1) {
      Str_Format(limits, sizeof(limits), "%d frags", entry->fragLimit);
    }
    if (entry->captureLimit > -1) {
      Str_Format(limits + strlen(limits), sizeof(limits) - strlen(limits), "%s%d captures",
                 *limits ? ", " : "", entry->captureLimit);
    }
    if (entry->timeLimit > -1) {
      Str_Format(limits + strlen(limits), sizeof(limits) - strlen(limits), "%s%d minutes",
                 *limits ? ", " : "", entry->timeLimit);
    }
    $(self->limits, setText, *limits ? limits : "The level's limits");

    const bool cleared = Cg_Practice_IsCleared(entry, selectedGamePlay(self));
    $(self->status, setText, cleared ? "Cleared" : "Not cleared");
  } else {
    $(self->map, setText, "Select a match");
    $(self->opponents, setNames, "");
    $(self->limits, setText, NULL);
    $(self->status, setText, NULL);
  }

  setEnabled((Control *) self->play, entry != NULL);
}

/**
 * @brief Plays the selected entry under the selected gameplay.
 */
static void play(PracticeViewController *self) {

  if (isStale(self)) {
    return;
  }

  const CGamePracticeEntry *entry = entryAtIndex(self->selected);
  if (entry) {
    Cg_Practice_Start(entry, selectedGamePlay(self));
  }
}

/**
 * @brief CollectionViewDataSource for a tier: the number of its entries.
 */
static size_t numberOfItems(const CollectionView *collectionView) {

  const PracticeTier *tier = collectionView->dataSource.self;
  return tier->numEntries;
}

/**
 * @brief CollectionViewDataSource for a tier: one of its entries.
 */
static ident objectForItemAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const PracticeTier *tier = collectionView->dataSource.self;
  return (ident) tier->entries[$(indexPath, indexAtPosition, 0)];
}

/**
 * @brief CollectionViewDelegate for a tier: a tile of the entry's map, marked if the player
 * cleared it under the selected gameplay.
 */
static CollectionItemView *itemForObjectAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const PracticeTier *tier = collectionView->dataSource.self;
  const CGamePracticeEntry *entry = tier->entries[$(indexPath, indexAtPosition, 0)];

  size_t count;
  const CGamePracticeEntry *ladder = Cg_Practice_Ladder(&count);

  MapListCollectionItemView *item = $(alloc(MapListCollectionItemView), initWithFrame, NULL);
  assert(item);

  $(item, setMapListItemInfo, &tier->self->maps[entry - ladder]);

  if (Cg_Practice_IsCleared(entry, selectedGamePlay(tier->self))) {
    $((View *) item, addClassName, "cleared");
  }

  return (CollectionItemView *) item;
}

/**
 * @brief CollectionViewDelegate for a tier: selects its entry, clears the selection of the other
 * tiers, and plays the entry on a double click.
 */
static void didModifySelection(CollectionView *collectionView, const Array *selectionIndexPaths) {

  PracticeTier *tier = collectionView->dataSource.self;
  PracticeViewController *this = tier->self;

  if (isStale(this)) {
    return;
  }

  for (size_t i = 0; i < this->numTiers; i++) {
    if (this->tier[i].collectionView != collectionView) {
      $(this->tier[i].collectionView, deselectAll);
    }
  }

  this->selected = -1;

  if (selectionIndexPaths->count) {
    const IndexPath *indexPath = $(selectionIndexPaths, objectAtIndex, 0);

    size_t count;
    const CGamePracticeEntry *ladder = Cg_Practice_Ladder(&count);

    this->selected = (int32_t) (tier->entries[$(indexPath, indexAtPosition, 0)] - ladder);
  }

  updateDetails(this);

  if (this->selected > -1) {
    const SDL_PropertiesID props = SDL_GetWindowProperties(((View *) collectionView)->window);
    const SDL_Event *event = SDL_GetPointerProperty(props, "event", NULL);
    if (event && event->button.clicks == 2) {
      play(this);
    }
  }
}

/**
 * @brief Reads the map of each entry of the ladder from its BSP, if the ladder changed.
 */
static void loadMaps(PracticeViewController *self) {

  size_t count;
  const CGamePracticeEntry *ladder = Cg_Practice_Ladder(&count);

  const uint32_t generation = Cg_Practice_LadderGeneration();
  if (generation == self->mapsGeneration && self->maps) {
    return;
  }

  free(self->maps);

  self->maps = calloc(Maxi(1, (int32_t) count), sizeof(MapListItemInfo));
  assert(self->maps);

  self->mapsGeneration = generation;

  for (size_t i = 0; i < count; i++) {
    MapListItemInfo *info = &self->maps[i];

    if (!Cg_LoadMapListItemInfo(va("maps/%s.bsp", ladder[i].name), info)) {
      Str_Copy(info->mapname, ladder[i].name, sizeof(info->mapname));
      Str_Copy(info->message, ladder[i].name, sizeof(info->message));
    }

    info->games[0] = '\0';
  }
}

/**
 * @brief Builds one row for each tier of the ladder, and selects the selected entry again.
 */
static void reloadTiers(PracticeViewController *self) {

  $((View *) self->tiers, removeAllSubviews);

  memset(self->tier, 0, sizeof(self->tier));
  self->numTiers = 0;

  loadMaps(self);

  self->tiersGeneration = Cg_Practice_LadderGeneration();

  size_t count;
  const CGamePracticeEntry *ladder = Cg_Practice_Ladder(&count);

  if (self->selected >= (int32_t) count) {
    self->selected = -1;
  }

  for (size_t i = 0; i < count; i++) {
    const CGamePracticeEntry *entry = &ladder[i];

    PracticeTier *tier = NULL;
    for (size_t j = 0; j < self->numTiers; j++) {
      if (self->tier[j].tier == entry->tier) {
        tier = &self->tier[j];
        break;
      }
    }

    if (tier == NULL) {
      tier = &self->tier[self->numTiers++];
      tier->self = self;
      tier->tier = entry->tier;
    }

    tier->entries[tier->numEntries++] = entry;
  }

  for (size_t i = 0; i < self->numTiers; i++) {
    PracticeTier *tier = &self->tier[i];

    Box *box = $(alloc(Box), initWithFrame, NULL);
    assert(box);

    $((View *) box, addClassName, "practiceTier");
    $(box->label->text, setTextWithFormat, "Tier %d", tier->tier);

    tier->collectionView = $(alloc(CollectionView), initWithFrame, NULL);
    assert(tier->collectionView);

    tier->collectionView->dataSource.self = tier;
    tier->collectionView->dataSource.numberOfItems = numberOfItems;
    tier->collectionView->dataSource.objectForItemAtIndexPath = objectForItemAtIndexPath;

    tier->collectionView->delegate.self = tier;
    tier->collectionView->delegate.itemForObjectAtIndexPath = itemForObjectAtIndexPath;
    tier->collectionView->delegate.didModifySelection = didModifySelection;

    $((View *) box->contentView, addSubview, (View *) tier->collectionView);
    release(tier->collectionView);

    $((View *) self->tiers, addSubview, (View *) box);
    release(box);

    $(tier->collectionView, reloadData);

    for (size_t j = 0; j < tier->numEntries; j++) {
      if (tier->entries[j] - ladder == self->selected) {
        IndexPath *indexPath = $(alloc(IndexPath), initWithIndex, j);
        $(tier->collectionView, selectItemAtIndexPath, indexPath);
        release(indexPath);
      }
    }
  }

  updateDetails(self);
}

#pragma mark - Delegates

/**
 * @brief SelectDelegate for the ladder Select: shows the ladder selected.
 */
static void didSelectLadder(Select *select, Option *option) {

  PracticeViewController *this = select->delegate.self;

  cgi.SetCvarString(cg_practiceLadder->name, option->value);

  this->selected = -1;

  updateGamePlay(this);
  reloadTiers(this);
}

/**
 * @brief SelectDelegate for the gameplay Select: marks the entries cleared under it.
 */
static void didSelectGamePlay(Select *select, Option *option) {
  reloadTiers(select->delegate.self);
}

/**
 * @brief ButtonDelegate for the Play button.
 */
static void didClickPlay(Button *button) {
  play(button->delegate.self);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  PracticeViewController *this = (PracticeViewController *) self;

  free(this->maps);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  PracticeViewController *this = (PracticeViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("ladder", &this->ladder),
    MakeOutlet("gameplay", &this->gameplay),
    MakeOutlet("tiers", &this->tiers),
    MakeOutlet("map", &this->map),
    MakeOutlet("opponents", &this->opponents),
    MakeOutlet("limits", &this->limits),
    MakeOutlet("status", &this->status),
    MakeOutlet("play", &this->play)
  );

  $(self->view, awakeWithResourceName, "ui/play/PracticeViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/play/PracticeViewController.css");
  assert(self->view->stylesheet);

  this->selected = -1;

  size_t numLadders;
  const CGamePracticeLadder *ladders = Cg_Practice_Ladders(&numLadders);

  for (size_t i = 0; i < numLadders; i++) {
    $(this->ladder, addOption, ladders[i].title, (ident) ladders[i].name);

    if (!Str_Compare(ladders[i].name, cg_practiceLadder->string)) {
      $(this->ladder, selectOptionWithValue, (ident) ladders[i].name);
    }
  }

  this->ladder->delegate.self = this;
  this->ladder->delegate.didSelectOption = didSelectLadder;

  size_t numGamePlays;
  const GamePlay *gamePlays = Cg_ListGamePlays(&numGamePlays);

  for (size_t i = 0; i < numGamePlays; i++) {
    if (!(gamePlays[i].id & GAMEPLAY_TEAMS)) {
      $(this->gameplay, addOption, gamePlays[i].label, (ident) gamePlays[i].name);
    }
  }

  this->gameplay->delegate.self = this;
  this->gameplay->delegate.didSelectOption = didSelectGamePlay;

  updateGamePlay(this);

  this->play->delegate.self = this;
  this->play->delegate.didClick = didClickPlay;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  reloadTiers((PracticeViewController *) self);
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
 * @fn Class *PracticeViewController::_PracticeViewController(void)
 * @memberof PracticeViewController
 */
Class *_PracticeViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "PracticeViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(PracticeViewController),
      .interfaceSize = sizeof(PracticeViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
