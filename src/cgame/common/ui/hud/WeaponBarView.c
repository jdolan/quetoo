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
#include "WeaponBarView.h"

#define _Class _WeaponBarView

#define WEAPON_BAR_SPACING 4

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  WeaponBarView *this = (WeaponBarView *) self;

  release(this->name);
  release(this->slots);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = (View *) super(StackView, self, initWithFrame, NULL);
  if (self) {
    WeaponBarView *this = (WeaponBarView *) self;

    this->stackView.axis = StackViewAxisVertical;

    this->name = $(alloc(Text), initWithText, NULL, NULL);
    assert(this->name);

    $(self, addSubview, (View *) this->name);

    this->slots = $(alloc(StackView), initWithFrame, NULL);
    assert(this->slots);

    this->slots->axis = StackViewAxisHorizontal;
    this->slots->spacing = WEAPON_BAR_SPACING;
    this->slots->view.autoresizingMask = ViewAutoresizingContain;

    $(self, addSubview, (View *) this->slots);
  }

  return self;
}

/**
 * @brief Rebuilds the slot row for the weapons carried.
 */
static void rebuild(WeaponBarView *self) {

  $((View *) self->slots, removeAllSubviews);

  for (int32_t i = 0; i < WEAPON_TOTAL; i++) {
    if (self->has[i]) {
      View *slot = $(alloc(View), initWithFrame, NULL);
      assert(slot);

      slot->autoresizingMask = ViewAutoresizingContain;
      $(slot, addClassName, "slot");

      ImageView *icon = $(alloc(ImageView), initWithFrame, &MakeRect(0, 0, HUD_PIC_HEIGHT, HUD_PIC_HEIGHT));
      assert(icon);

      const char *name = bg_item_defs[cg_weapons[i].tag].icon;
      $(icon, setImage, name ? (Image *) Cg_HudImage(name) : NULL);

      $(slot, addSubview, (View *) icon);
      release(icon);

      $((View *) self->slots, addSubview, slot);
      release(slot);
    }
  }

  self->selected = self->slots->view.subviews->count;
}

/**
 * @brief Moves the `selected` class name to the slot at the given index, or clears it when
 * the index is the slot count.
 */
static void selectSlot(WeaponBarView *self, size_t index) {

  const Array *slots = (Array *) self->slots->view.subviews;

  if (index == self->selected) {
    return;
  }

  if (self->selected < slots->count) {
    View *slot = $(slots, objectAtIndex, self->selected);
    $(slot, removeClassName, "selected");
  }

  if (index < slots->count) {
    View *slot = $(slots, objectAtIndex, index);
    $(slot, addClassName, "selected");
  }

  self->selected = index;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  WeaponBarView *this = (WeaponBarView *) self;

  if (data == NULL) {
    return;
  }

  const player_state_t *ps = &((const cl_frame_t *) data)->ps;

  float alpha;
  const bool visible = Cg_UpdateSelectWeapon(ps, &alpha);

  $(self, setHidden, !visible);

  if (!visible) {
    return;
  }

  if (memcmp(this->has, cg_hud_state.weapon.has, sizeof(this->has))) {
    memcpy(this->has, cg_hud_state.weapon.has, sizeof(this->has));
    rebuild(this);
  }

  const Uint8 selected = (Uint8) (alpha * 255);
  const Uint8 unselected = (Uint8) (alpha * cg_select_weapon_alpha->value * 255);

  const Array *slots = (Array *) this->slots->view.subviews;

  size_t index = slots->count;

  for (int32_t i = 0, k = 0; i < WEAPON_TOTAL; i++) {
    if (!this->has[i]) {
      continue;
    }

    const View *slot = $(slots, objectAtIndex, k);
    ImageView *icon = $((Array *) slot->subviews, firstObject);

    if (i == cg_hud_state.weapon.bit) {
      icon->color.a = selected;

      index = k;

      $(this->name, setText, bg_item_defs[cg_weapons[i].tag].name);
    } else {
      icon->color.a = unselected;
    }

    k++;
  }

  selectSlot(this, index);

  if (index == slots->count) {
    $(this->name, setText, NULL);
  }

  this->name->color.a = selected;
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *WeaponBarView::_WeaponBarView(void)
 * @memberof WeaponBarView
 */
Class *_WeaponBarView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "WeaponBarView",
      .superclass = _StackView(),
      .instanceSize = sizeof(WeaponBarView),
      .interfaceSize = sizeof(WeaponBarViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
