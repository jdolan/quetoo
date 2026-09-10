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

#include "NavEditView.h"

#define _Class _NavEditView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  NavEditView *this = (NavEditView *) self;

  release(this->text);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = super(View, self, initWithFrame, &MakeRect(24, 32, 0, 0));
  if (self) {
    NavEditView *this = (NavEditView *) self;

    $(self->style, addRectangleAttribute, "padding", &MakeRect(8, 16, 8, 16));
    $(self->style, addColorAttribute, "background-color", &(const SDL_Color) { 0, 0, 0, 166 });
    $(self->style, addIntegerAttribute, "border-radius", 4);

    this->text = $(alloc(Text), initWithText, NULL, NULL);
    assert(this->text);

    $(this->text->view.style, addCharactersAttribute, "font-family", DEFAULT_MONOSPACE_FONT_FAMILY);
    $(this->text->view.style, addIntegerAttribute, "font-size", 14);

    $(self, addSubview, (View *) this->text);
  }

  return self;
}

/**
 * @brief The colored key name bound to the given command, or red `UNBOUND`.
 */
static const char *keyBind(const char *bind) {

  const SDL_Scancode code = cgi.KeyForBind(SDL_SCANCODE_UNKNOWN, bind);

  if (code == SDL_SCANCODE_UNKNOWN) {
    return "^1UNBOUND^7";
  }

  return va("^2%s^7", cgi.KeyName(code));
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  NavEditView *this = (NavEditView *) self;

  const bool shown = cg_state.nav_edit == 1;

  $(self, setVisibility, shown ? ViewVisibilityVisible : ViewVisibilityHidden);

  if (!shown) {
    return;
  }

  char text[1024];
  snprintf(text, sizeof(text),
    "NAVIGATION EDIT MODE\n"
    "You're in nav edit mode; items can't be picked up,\n"
    "you can't die, and the map will never end.\n"
    "* To start placing/linking nav nodes, press %s. A node will\n"
    "  drop at your location, and you can now place them\n"
    "  by running around like you normally would.\n"
    "* To stop placing/linking nodes, press %s again.\n"
    "* To change a node's position, select the node by touching it\n"
    "  so it turns yellow, and press %s.\n"
    "* To delete a node, select the node by touching it\n"
    "  so it turns yellow, and press %s.\n"
    "* To adjust the link state between two nodes, select\n"
    "  the nodes by touching them so they turn yellow & purple respectively\n"
    "  then tap %s to cycle between connection types.",
    keyBind("+attack"), keyBind("+attack"), keyBind("use"), keyBind("+hook"), keyBind("+score"));

  $(this->text, setText, text);
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *NavEditView::_NavEditView(void)
 * @memberof NavEditView
 */
Class *_NavEditView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "NavEditView",
      .superclass = _View(),
      .instanceSize = sizeof(NavEditView),
      .interfaceSize = sizeof(NavEditViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
