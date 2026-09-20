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

#include "DemosViewController.h"

#define _Class _DemosViewController

/**
 * @brief Returns `name` without its `"demos/"` directory and `".demo"` extension, suitable for
 * the `demo` server command, which resolves both itself.
 */
static void demoCommandName(const char *filename, char *out, size_t len) {
  StripExtension(Basename(filename), out);

  // the filename ultimately comes from disk (this client's own recording, or one shared by
  // someone else) and is about to be interpolated into a Cbuf command string; sanitize it the
  // same way Cg_DiscordJoinGame does for its own untrusted string, so a maliciously named demo
  // file can't inject an additional console command when played
  for (char *c = out; *c; c++) {
    if (*c == '\n' || *c == ';' || *c == '"') {
      *c = '\0';
      break;
    }
  }

  (void) len;
}

#pragma mark - Delegates

/**
 * @brief TextViewDelegate for the map name filter field.
 */
static void filterDidEndEditing(TextView *textView) {

  DemosViewController *this = textView->delegate.self;

  const String *string = (String *) textView->attributedText;
  $(this->demosList, setFilter, string ? string->chars : NULL);
}

/**
 * @brief Plays the given demo via the `demo` server command.
 */
static void playDemo(const DemoListItemInfo *info) {

  char name[MAX_QPATH];
  demoCommandName(info->filename, name, sizeof(name));

  cgi.Cbuf(va("demo \"%s\"\n", name));
}

/**
 * @brief ButtonDelegate for the Play button.
 */
static void didClickPlay(Button *button) {

  DemosViewController *this = button->delegate.self;

  const DemoListItemInfo *info = $(this->demosList, selectedDemo);
  if (info) {
    playDemo(info);
  }
}

/**
 * @brief Enables the Play button only when a demo is selected, since it has nothing to play
 * otherwise and would appear clickable while doing nothing.
 */
static void updatePlay(DemosViewController *self, bool selected) {

  Control *control = (Control *) self->play;

  const ControlState state = selected
    ? control->state & ~ControlStateDisabled
    : control->state | ControlStateDisabled;

  if (state == control->state) {
    return;
  }

  control->state = state;

  // the style carries the disabled appearance, and only stateDidChange asks for it to be
  // recomputed. Without this the button keeps the look it had until a pointer event over it
  // happens to drive the same path
  $(control, stateDidChange);
}

/**
 * @brief CollectionViewDelegate for the demos list: double-clicking a demo plays it.
 */
static void didModifySelection(CollectionView *collectionView, const Array *selectionIndexPaths) {

  DemosViewController *this = collectionView->delegate.self;

  updatePlay(this, selectionIndexPaths->count != 0);

  if (selectionIndexPaths->count == 0) {
    return;
  }

  const SDL_PropertiesID props = SDL_GetWindowProperties(((View *) collectionView)->window);
  const SDL_Event *event = SDL_GetPointerProperty(props, "event", NULL);
  if (event && event->button.clicks == 2) {
    const DemoListItemInfo *info = $(this->demosList, selectedDemo);
    if (info) {
      playDemo(info);
    }
  }
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  DemosViewController *this = (DemosViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("demosList", &this->demosList),
    MakeOutlet("demosEmpty", &this->emptyLabel),
    MakeOutlet("filter", &this->filter),
    MakeOutlet("play", &this->play)
  );

  $(self->view, awakeWithResourceName, "ui/home/DemosViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/home/DemosViewController.css");
  assert(self->view->stylesheet);

  this->demosList->emptyStateView = (View *) this->emptyLabel;

  this->demosList->collectionView.delegate.self = this;
  this->demosList->collectionView.delegate.didModifySelection = didModifySelection;

  this->filter->delegate.self = this;
  this->filter->delegate.didEndEditing = filterDidEndEditing;

  this->play->delegate.self = this;
  this->play->delegate.didClick = didClickPlay;

  updatePlay(this, false);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  DemosViewController *this = (DemosViewController *) self;

  $(this->demosList, reloadDemos);

  // a reload drops the selection without telling the delegate, so a demo deleted from under us
  // would otherwise leave Play enabled with nothing to play
  updatePlay(this, $(this->demosList, selectedDemo) != NULL);

  super(ViewController, self, viewWillAppear);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

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
      .interfaceSize = sizeof(DemosViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
