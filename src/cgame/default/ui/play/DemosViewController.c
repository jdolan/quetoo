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

#include "DemosViewController.h"

#define _Class _DemosViewController

static const char *_demo = "demo";

#pragma mark - Demo enumeration

/**
 * @brief Fs_Enumerator that appends each demo's base name (sans extension).
 */
static void enumerateDemos(const char *path, void *data) {

  DemosViewController *this = data;

  char *base = g_path_get_basename(path);
  char *dot = strrchr(base, '.');
  if (dot) {
    *dot = '\0';
  }

  this->names = g_list_append(this->names, g_strdup(base));
  g_free(base);
}

/**
 * @brief Rebuilds the demo list from the filesystem.
 */
static void loadDemos(DemosViewController *this) {

  g_list_free_full(this->names, g_free);
  this->names = NULL;
  this->selectedRow = -1;

  cgi.EnumerateFiles("demos/*.demo", enumerateDemos, this);

  this->names = g_list_sort(this->names, (GCompareFunc) g_strcmp0);
}

/**
 * @brief Plays the currently selected demo, if any.
 */
static void playSelected(DemosViewController *this) {

  if (this->selectedRow < 0) {
    return;
  }

  const char *name = g_list_nth_data(this->names, (guint) this->selectedRow);
  if (name) {
    cgi.Cbuf(va("demo \"%s\"\n", name));
  }
}

#pragma mark - TableView dataSource / delegate

static size_t numberOfRows(const TableView *tableView) {
  const DemosViewController *this = tableView->dataSource.self;
  return g_list_length(this->names);
}

static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  const DemosViewController *this = tableView->dataSource.self;
  const char *name = g_list_nth_data(this->names, (guint) row);

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);
  $(cell->text, setText, name);

  return cell;
}

static void didSelectRowsAtIndexes(TableView *tableView, const IndexSet *indexes) {

  DemosViewController *this = tableView->delegate.self;

  this->selectedRow = indexes->count ? (int32_t) indexes->indexes[0] : -1;

  // double-click to play (mirrors JoinServerViewController)
  const View *view = (View *) tableView;
  const SDL_PropertiesID props = SDL_GetWindowProperties(view->window);
  const SDL_Event *event = SDL_GetPointerProperty(props, "event", NULL);
  if (event && event->button.clicks == 2) {
    playSelected(this);
  }
}

#pragma mark - Button delegates

static void didClickPlay(Button *button) {
  playSelected((DemosViewController *) button->delegate.self);
}

static void didClickRefresh(Button *button) {

  DemosViewController *this = button->delegate.self;

  loadDemos(this);
  $(this->demos, reloadData);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  DemosViewController *this = (DemosViewController *) self;

  Button *play, *refresh;
  Outlet outlets[] = MakeOutlets(
    MakeOutlet("demos", &this->demos),
    MakeOutlet("play", &play),
    MakeOutlet("refresh", &refresh)
  );

  $(self->view, awakeWithResourceName, "ui/play/DemosViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/play/DemosViewController.css");
  assert(self->view->stylesheet);

  $(this->demos, addColumnWithIdentifier, _demo);

  this->demos->dataSource.numberOfRows = numberOfRows;
  this->demos->dataSource.self = this;

  this->demos->delegate.cellForColumnAndRow = cellForColumnAndRow;
  this->demos->delegate.didSelectRowsAtIndexes = didSelectRowsAtIndexes;
  this->demos->delegate.self = this;

  play->delegate.didClick = didClickPlay;
  play->delegate.self = this;

  refresh->delegate.didClick = didClickRefresh;
  refresh->delegate.self = this;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  DemosViewController *this = (DemosViewController *) self;

  loadDemos(this);
  $(this->demos, reloadData);
}

#pragma mark - Object

static void dealloc(Object *self) {

  DemosViewController *this = (DemosViewController *) self;

  g_list_free_full(this->names, g_free);

  super(Object, self, dealloc);
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
 * @fn Class *DemosViewController::_DemosViewController(void)
 * @memberof DemosViewController
 */
Class *_DemosViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "DemosViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(DemosViewController),
      .interfaceOffset = offsetof(DemosViewController, interface),
      .interfaceSize = sizeof(DemosViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
