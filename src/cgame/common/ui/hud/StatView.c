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
#include "StatView.h"

#define _Class _StatView

static const EnumName StatViewStatNames[] = MakeEnumNames(
  MakeEnumAlias(StatViewHealth, health),
  MakeEnumAlias(StatViewArmor, armor),
  MakeEnumAlias(StatViewAmmo, ammo)
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

  StatView *this = (StatView *) self;

  release(this->icon);
  release(this->value);

  super(Object, self, dealloc);
}

#pragma mark - Icons

/**
 * @brief The health icon for the given value, by the thresholds Cg_HealthIcon used.
 */
static const char *healthIconName(int16_t health) {

  if (health > 100) {
    return "pics/i_health_mega";
  } else if (health > 75) {
    return "pics/i_health";
  } else if (health > 25) {
    return "pics/i_health_medium";
  } else {
    return "pics/i_health_large";
  }
}

/**
 * @brief The icon of the best armor carried, or `NULL`.
 */
static const char *armorIconName(const player_state_t *ps) {

  for (g_item_tag_t t = ARMOR_QUAKE_BODY; t > ARMOR_SHARD; t--) {
    if (ps->inventory[t]) {
      return bg_item_defs[t].icon;
    }
  }

  return NULL;
}

#pragma mark - View

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

  super(View, self, awakeWithDictionary, dictionary);

  StatView *this = (StatView *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("stat", InletTypeEnum, &this->stat, (ident) StatViewStatNames)
  );

  $(self, bind, inlets, dictionary);
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((StatView *) self, initWithStat, StatViewHealth);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  StatView *this = (StatView *) self;

  if (data == NULL) {
    $(this->icon, setImage, NULL);
    this->iconName = NULL;
    return;
  }

  const player_state_t *ps = &((const cl_frame_t *) data)->ps;

  int16_t value = 0, med = -1, low = -1;
  const char *iconName = NULL;

  switch (this->stat) {
    case StatViewHealth:
      value = ps->stats[STAT_HEALTH];
      med = HUD_HEALTH_MED;
      low = HUD_HEALTH_LOW;
      iconName = healthIconName(value);
      break;
    case StatViewArmor:
      if ((cg_state.gameplay & ~GAMEPLAY_TEAMS) != GAMEPLAY_INSTAGIB) {
        value = ps->stats[STAT_ARMOR];
        med = HUD_ARMOR_MED;
        low = HUD_ARMOR_LOW;
        iconName = armorIconName(ps);
      }
      break;
    case StatViewAmmo:
      if ((cg_state.gameplay & ~GAMEPLAY_TEAMS) != GAMEPLAY_INSTAGIB) {
        value = Cg_ActiveAmmo(ps);

        const int16_t active = Cg_ActiveWeapon(ps);
        if (active != WEAPON_SELECT_OFF) {
          low = (int16_t) bg_item_defs[cg_weapons[active].ammo_tag].quantity;
          iconName = bg_item_defs[cg_weapons[active].tag].icon;
        }
      }
      break;
  }

  $(self, setHidden, value <= 0);

  if (value <= 0) {
    return;
  }

  SDL_Color color = Colors.White;
  float pulse = 1.f;

  if (value < low) {
    color = Colors.Red;
    if (cg_draw_vitals_pulse->integer) {
      pulse = Clampf(sinf(cgi.client->unclamped_time / 250.f), 0.75f, 1.f);
    }
  } else if (value < med) {
    color = Colors.Yellow;
  }

  setTextColor(this->value, color);
  $(this->value, setTextWithFormat, "%3d", value);

  this->icon->color.a = (Uint8) (pulse * 255);

  if (iconName != this->iconName) {
    this->iconName = iconName;
    $(this->icon, setImage, iconName ? (Image *) Cg_HudImage(iconName) : NULL);
  }
}

#pragma mark - StatView

/**
 * @fn StatView *StatView::initWithStat(StatView *self, StatViewStat stat)
 * @memberof StatView
 */
static StatView *initWithStat(StatView *self, StatViewStat stat) {

  self = (StatView *) super(StackView, self, initWithFrame, NULL);
  if (self) {
    self->stat = stat;

    self->stackView.axis = StackViewAxisHorizontal;
    self->stackView.spacing = 5;

    self->value = $(alloc(Text), initWithText, NULL, NULL);
    assert(self->value);

    $((View *) self, addSubview, (View *) self->value);

    self->icon = $(alloc(ImageView), initWithFrame, &MakeRect(0, 0, HUD_PIC_HEIGHT, HUD_PIC_HEIGHT));
    assert(self->icon);

    $((View *) self, addSubview, (View *) self->icon);
  }

  return self;
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((StatViewInterface *) clazz->interface)->initWithStat = initWithStat;
}

/**
 * @fn Class *StatView::_StatView(void)
 * @memberof StatView
 */
Class *_StatView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "StatView",
      .superclass = _StackView(),
      .instanceSize = sizeof(StatView),
      .interfaceSize = sizeof(StatViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
