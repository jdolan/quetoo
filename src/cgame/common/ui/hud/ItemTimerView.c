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

#include "ItemTimerView.h"

#define _Class _ItemTimerView

static const EnumName itemNames[] = MakeEnumNames(
  MakeEnumAlias(ItemTimerViewMega, mega),
  MakeEnumAlias(ItemTimerViewQuad, quad),
  MakeEnumAlias(ItemTimerViewInvulnerability, invulnerability),
  MakeEnumAlias(ItemTimerViewInvisibility, invisibility)
);

#pragma mark - View

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

  super(View, self, awakeWithDictionary, dictionary);

  ItemTimerView *this = (ItemTimerView *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("item", InletTypeEnum, &this->item, (ident) itemNames)
  );

  $(self, bind, inlets, dictionary);
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((PowerupView *) self, initWithPowerup, PowerupViewNone);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  if (data == NULL) {
    return;
  }

  ItemTimerView *this = (ItemTimerView *) self;

  GameItemTag item = ITEM_NONE;
  int32_t index = 0;

  switch (this->item) {
    case ItemTimerViewMega:
      item = cgameState.items == ITEMS_QUAKE ? HEALTH_QUAKE_MEGA : HEALTH_MEGA;
      index = CS_ITEM_TIMER_MEGA;
      break;
    case ItemTimerViewQuad:
      item = POWERUP_QUAD;
      index = CS_ITEM_TIMER_QUAD;
      break;
    case ItemTimerViewInvulnerability:
      item = POWERUP_INVULNERABILITY;
      index = CS_ITEM_TIMER_INVULNERABILITY;
      break;
    case ItemTimerViewInvisibility:
      item = POWERUP_INVISIBILITY;
      index = CS_ITEM_TIMER_INVISIBILITY;
      break;
  }

  $((PowerupView *) self, update, item, (int16_t) atoi(cgi.ConfigString(index)));
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *ItemTimerView::_ItemTimerView(void)
 * @memberof ItemTimerView
 */
Class *_ItemTimerView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ItemTimerView",
      .superclass = _PowerupView(),
      .instanceSize = sizeof(ItemTimerView),
      .interfaceSize = sizeof(ItemTimerViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
