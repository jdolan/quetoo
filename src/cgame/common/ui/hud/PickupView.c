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

#include "HudViewController.h"
#include "PickupView.h"

#define _Class _PickupView

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = (View *) super(StackView, self, initWithFrame, NULL);
  if (self) {
    PickupView *this = (PickupView *) self;

    this->item = ITEM_NONE;

    Outlet outlets[] = MakeOutlets(
      MakeOutlet("icon", &this->icon),
      MakeOutlet("name", &this->name)
    );

    $(self, awakeWithResourceName, "ui/hud/PickupView.json");
    $(self, resolve, outlets);

    this->icon->view.frame = MakeRect(0, 0, HUD_PIC_HEIGHT, HUD_PIC_HEIGHT);
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
    return;
  }

  const PlayerState *ps = &((const ClientFrame *) data)->ps;

  const int16_t pickup = ps->stats[STAT_PICKUP] & ~STAT_TOGGLE_BIT;
  const bool valid = pickup > ITEM_NONE && pickup < ITEM_TOTAL;

  $(self, setVisibility, valid ? ViewVisibilityVisible : ViewVisibilityHidden);

  if (valid && pickup != (int16_t) this->item) {
    this->item = pickup;

    const char *icon = bgItemDefs[pickup].icon;
    $(this->icon, setImage, icon ? (Image *) Cg_HudImage(icon) : NULL);
    $(this->name, setText, bgItemDefs[pickup].name);
  }
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

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
