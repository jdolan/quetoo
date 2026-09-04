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

#include "HudViewController.h"
#include "PickupView.h"

#define _Class _PickupView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  PickupView *this = (PickupView *) self;

  release(this->icon);
  release(this->name);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = super(View, self, initWithFrame, NULL);
  if (self) {
    PickupView *this = (PickupView *) self;

    this->item = ITEM_NONE;

    this->stackView.axis = StackViewAxisHorizontal;

    this->icon = $(alloc(ImageView), initWithFrame, &MakeRect(0, 0, HUD_PIC_HEIGHT, HUD_PIC_HEIGHT));
    assert(this->icon);

    $(self, addSubview, (View *) this->icon);

    this->name = $(alloc(Text), initWithText, NULL, NULL);
    assert(this->name);

    this->name->view.alignment = ViewAlignmentMiddle;

    $(self, addSubview, (View *) this->name);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  PickupView *this = (PickupView *) self;

  if (data == NULL) {
    $(this->icon, setImage, NULL);
    this->item = ITEM_NONE;
    return;
  }

  const player_state_t *ps = &((const cl_frame_t *) data)->ps;

  const int16_t pickup = ps->stats[STAT_PICKUP] & ~STAT_TOGGLE_BIT;
  const bool valid = pickup > ITEM_NONE && pickup < ITEM_TOTAL;

  $(self, setHidden, !valid);

  if (valid && pickup != (int16_t) this->item) {
    this->item = pickup;

    const char *icon = bg_item_defs[pickup].icon;
    $(this->icon, setImage, icon ? (Image *) Cg_HudImage(icon) : NULL);
    $(this->name, setText, bg_item_defs[pickup].name);
  }
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *PickupView::_PickupView(void)
 * @memberof PickupView
 */
Class *_PickupView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "PickupView",
      .superclass = _StackView(),
      .instanceSize = sizeof(PickupView),
      .interfaceSize = sizeof(PickupViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
