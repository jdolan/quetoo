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
#include "PowerupView.h"

#define _Class _PowerupView

static const EnumName PowerupViewPowerupNames[] = MakeEnumNames(
  MakeEnumAlias(PowerupViewQuad, quad),
  MakeEnumAlias(PowerupViewInvulnerability, invulnerability),
  MakeEnumAlias(PowerupViewInvisibility, invisibility)
);

/**
 * @brief Sets the Text's colour. Free on the BitmapFont path; on the Font path the colour
 * is baked into the texture, which must be dropped for the change to show.
 */
static void setTextColor(Text *text, SDL_Color color) {

  if (memcmp(&text->color, &color, sizeof(color))) {
    text->color = color;
    text->texture = release(text->texture);
  }
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  PowerupView *this = (PowerupView *) self;

  release(this->icon);
  release(this->value);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

  super(View, self, awakeWithDictionary, dictionary);

  PowerupView *this = (PowerupView *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("powerup", InletTypeEnum, &this->powerup, (ident) PowerupViewPowerupNames)
  );

  $(self, bind, inlets, dictionary);
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((PowerupView *) self, initWithPowerup, PowerupViewQuad);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  PowerupView *this = (PowerupView *) self;

  if (data == NULL) {
    $(this->icon, setImage, NULL);
    this->item = ITEM_NONE;
    return;
  }

  const player_state_t *ps = &((const cl_frame_t *) data)->ps;

  g_item_tag_t item = ITEM_NONE;
  int16_t value = 0;

  switch (this->powerup) {
    case PowerupViewNone:
      return;
    case PowerupViewQuad:
      item = POWERUP_QUAD;
      value = ps->stats[STAT_QUAD_TIME];
      break;
    case PowerupViewInvulnerability:
      item = POWERUP_INVULNERABILITY;
      value = ps->stats[STAT_INVULNERABILITY_TIME];
      break;
    case PowerupViewInvisibility:
      item = POWERUP_INVISIBILITY;
      value = ps->stats[STAT_INVISIBILITY_TIME];
      break;
  }

  $(this, update, item, value);
}

#pragma mark - PowerupView

/**
 * @fn void PowerupView::update(PowerupView *self, g_item_tag_t item, int16_t value)
 * @memberof PowerupView
 */
static void update(PowerupView *self, g_item_tag_t item, int16_t value) {

  const bool valid = item > ITEM_NONE && item < ITEM_TOTAL;

  $((View *) self, setHidden, value == 0 || !valid);

  if (value == 0 || !valid) {
    return;
  }

  if (item != self->item) {
    self->item = item;

    const char *icon = bg_item_defs[item].icon;
    $(self->icon, setImage, icon ? (Image *) Cg_HudImage(icon) : NULL);
  }

  if (value < 0) {
    $(self->value, setText, NULL);
  } else {
    setTextColor(self->value, value < HUD_POWERUP_LOW ? Colors.Red : Colors.White);
    $(self->value, setTextWithFormat, "%d", value);
  }
}

/**
 * @fn PowerupView *PowerupView::initWithPowerup(PowerupView *self, PowerupViewPowerup powerup)
 * @memberof PowerupView
 */
static PowerupView *initWithPowerup(PowerupView *self, PowerupViewPowerup powerup) {

  self = (PowerupView *) super(StackView, self, initWithFrame, NULL);
  if (self) {
    self->powerup = powerup;
    self->item = ITEM_NONE;

    self->stackView.axis = StackViewAxisHorizontal;
    self->stackView.spacing = HUD_PIC_HEIGHT / 2;

    self->icon = $(alloc(ImageView), initWithFrame, &MakeRect(0, 0, HUD_PIC_HEIGHT, HUD_PIC_HEIGHT));
    assert(self->icon);

    $((View *) self, addSubview, (View *) self->icon);

    self->value = $(alloc(Text), initWithText, NULL, NULL);
    assert(self->value);

    $((View *) self, addSubview, (View *) self->value);
  }

  return self;
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((PowerupViewInterface *) clazz->interface)->initWithPowerup = initWithPowerup;
  ((PowerupViewInterface *) clazz->interface)->update = update;
}

/**
 * @fn Class *PowerupView::_PowerupView(void)
 * @memberof PowerupView
 */
Class *_PowerupView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "PowerupView",
      .superclass = _StackView(),
      .instanceSize = sizeof(PowerupView),
      .interfaceSize = sizeof(PowerupViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
