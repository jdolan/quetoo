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

#include "ui/hud/HudViewController.h"
#include "ui/hud/PowerupView.h"

#define _Class _HeldFlagView

/**
 * @brief The flag the player carries, pulsing on a card like the tech's, or hidden.
 * @extends PowerupView
 */
typedef struct HeldFlagViewInterface HeldFlagViewInterface;

typedef struct {
  PowerupView powerupView;
  HeldFlagViewInterface *interface[0];
} HeldFlagView;

struct HeldFlagViewInterface {
  PowerupViewInterface powerupViewInterface;
};

/**
 * @see View::updateBindings(View *, ident)
 */
static void HeldFlagView_updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  if (data == NULL) {
    return;
  }

  PowerupView *this = (PowerupView *) self;
  const PlayerState *ps = &((const ClientFrame *) data)->ps;

  GameItemTag flag = ITEM_NONE;
  for (GameItemTag i = FLAG_FIRST; i < FLAG_LAST; i++) {
    if (ps->inventory[i]) {
      flag = i;
      break;
    }
  }

  // a card like the tech's, with no number
  $(this, update, flag, -1);

  if (flag != ITEM_NONE) {
    this->icon->color.a = (Uint8) (Clampf(sinf(cgi.client->unclampedTime / 150.f), 0.75f, 1.f) * 255);
  }
}

/**
 * @see View::init(View *)
 */
static View *HeldFlagView_init(View *self) {
  return (View *) $((PowerupView *) self, initWithPowerup, PowerupViewNone);
}

/**
 * @see Class::initialize(Class *)
 */
static void HeldFlagView_initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = HeldFlagView_init;
  ((ViewInterface *) clazz->interface)->updateBindings = HeldFlagView_updateBindings;
}

Class *_HeldFlagView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "HeldFlagView",
      .superclass = _PowerupView(),
      .instanceSize = sizeof(HeldFlagView),
      .interfaceSize = sizeof(HeldFlagViewInterface),
      .initialize = HeldFlagView_initialize,
    });
  });

  return clazz;
}

#undef _Class

#define _Class _FlagStatusView

/**
 * @brief Where one team's flag is, beside the clock: its icon, and beneath it "Base", "Taken" or
 * "Dropped". A taken flag pulses, and a dropped flag is dim.
 * @extends StackView
 */
typedef struct FlagStatusViewInterface FlagStatusViewInterface;

typedef struct {
  StackView stackView;
  FlagStatusViewInterface *interface[0];
  int32_t team;
  ImageView *icon;
  Text *label;
  char state;
} FlagStatusView;

struct FlagStatusViewInterface {
  StackViewInterface stackViewInterface;
};

/**
 * @see Object::dealloc(Object *)
 */
static void FlagStatusView_dealloc(Object *self) {

  FlagStatusView *this = (FlagStatusView *) self;

  release(this->icon);
  release(this->label);

  super(Object, self, dealloc);
}

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void FlagStatusView_awakeWithDictionary(View *self, const Dictionary *dictionary) {

  super(View, self, awakeWithDictionary, dictionary);

  FlagStatusView *this = (FlagStatusView *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("team", InletTypeInteger, &this->team, NULL)
  );

  $(self, bind, inlets, dictionary);
}

/**
 * @see View::init(View *)
 */
static View *FlagStatusView_init(View *self) {

  self = (View *) super(StackView, self, initWithFrame, NULL);
  if (self) {
    FlagStatusView *this = (FlagStatusView *) self;

    this->icon = $(alloc(ImageView), initWithFrame, &MakeRect(0, 0, HUD_PIC_HEIGHT, HUD_PIC_HEIGHT));
    assert(this->icon);

    $((View *) this->icon, addClassName, "icon");
    $(self, addSubview, (View *) this->icon);

    this->label = $(alloc(Text), initWithText, NULL, NULL);
    assert(this->label);

    $((View *) this->label, addClassName, "label");
    $(self, addSubview, (View *) this->label);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void FlagStatusView_updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  if (data == NULL) {
    return;
  }

  FlagStatusView *this = (FlagStatusView *) self;

  const char *states = cgi.ConfigString(CS_CTF_FLAGS);
  const int32_t numTeams = (int32_t) strlen(states);

  // teams 1 and 3 face teams 2 and 4 across the clock: a team that the level lacks keeps its
  // place while the team that faces it plays, so that the clock stays centered
  const int32_t facing = (this->team & 1) ? this->team + 1 : this->team - 1;

  const bool plays = this->team > 0 && this->team <= numTeams;
  const bool holds = plays || (facing > 0 && facing <= numTeams);

  $(self, setVisibility, holds ? ViewVisibilityVisible : ViewVisibilityHidden);

  if (!plays) {
    if (this->state != ' ') {
      this->state = ' ';
      this->icon->color.a = 0;
      $(this->label, setText, " ");
    }
    return;
  }

  const char state = states[this->team - 1];

  if (this->icon->image == NULL) {
    $(this->icon, setImage, (Image *) Cg_HudImage(va("pics/flag%d", this->team)));
  }

  if (state != this->state) {
    this->state = state;

    switch (state) {
      case CTF_FLAG_BASE:
        $(this->label, setText, "Base");
        break;
      case CTF_FLAG_CARRIED:
        $(this->label, setText, "Taken");
        break;
      default:
        $(this->label, setText, "Dropped");
        break;
    }
  }

  float alpha = 1.f;
  if (state == CTF_FLAG_CARRIED) {
    alpha = Clampf(sinf(cgi.client->unclampedTime / 150.f), 0.5f, 1.f);
  } else if (state == CTF_FLAG_DROPPED) {
    alpha = .4f;
  }

  this->icon->color.a = (Uint8) (alpha * 255);
}

/**
 * @see Class::initialize(Class *)
 */
static void FlagStatusView_initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = FlagStatusView_dealloc;

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = FlagStatusView_awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->init = FlagStatusView_init;
  ((ViewInterface *) clazz->interface)->updateBindings = FlagStatusView_updateBindings;
}

Class *_FlagStatusView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "FlagStatusView",
      .superclass = _StackView(),
      .instanceSize = sizeof(FlagStatusView),
      .interfaceSize = sizeof(FlagStatusViewInterface),
      .initialize = FlagStatusView_initialize,
    });
  });

  return clazz;
}

#undef _Class

/**
 * @brief Captures are team deathmatch or team instagib: arena does not apply,
 * and teams are not optional. A single owner, like the game side's
 * `G_ClampGamePlay_Ctf`, so it does not add to what `previous` offers.
 * @details The rows are copies of the shared `gameplays` table's, so their
 * `name` and `label` are the table's own strings.
 */
static const GamePlay *Cg_ListGamePlays_Ctf(size_t *count) {
  static GamePlay ctfGamePlays[2];

  *count = 0;

  for (size_t i = 0; i < lengthof(gameplays); i++) {
    if (gameplays[i].id == GAMEPLAY_TEAM_DEATHMATCH || gameplays[i].id == GAMEPLAY_TEAM_INSTAGIB) {
      ctfGamePlays[(*count)++] = gameplays[i];
    }
  }

  return ctfGamePlays;
}

/**
 * @brief Installs the capture the flag feature's client side, once per module
 * image.
 * @details The guard is load bearing rather than defensive: `Cg_Init` runs again
 * on a game change and on `r_restart`, and the client game image survives
 * `dlclose`, so a second install would point this function's previous at itself.
 */
void Cg_Ctf_Init(void) {
  static bool installed;

  if (installed) {
    return;
  }

  Cg_ListGamePlays = Cg_ListGamePlays_Ctf;

  installed = true;
}
