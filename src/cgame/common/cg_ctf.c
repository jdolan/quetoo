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
#include "ui/hud/TeamScoreView.h"

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
 * @brief A team's captures beside the clock, under the team's flag: "Base", "Stolen" or "Dropped"
 * over the captures. A stolen flag pulses, and a dropped flag is dim.
 * @extends TeamScoreView
 */
typedef struct FlagStatusViewInterface FlagStatusViewInterface;

typedef struct {
  TeamScoreView teamScoreView;
  FlagStatusViewInterface *interface[0];
  ImageView *icon;
} FlagStatusView;

struct FlagStatusViewInterface {
  TeamScoreViewInterface teamScoreViewInterface;
};

/**
 * @return Where the team's flag is, or '\0' if the level has no flag for it.
 */
static char FlagStatusView_flagState(const TeamScoreView *self) {

  const char *flag = strchr(self->state, ':');

  return flag ? flag[1] : '\0';
}

/**
 * @see Object::dealloc(Object *)
 */
static void FlagStatusView_dealloc(Object *self) {

  FlagStatusView *this = (FlagStatusView *) self;

  release(this->icon);

  super(Object, self, dealloc);
}

/**
 * @see View::init(View *)
 */
static View *FlagStatusView_init(View *self) {

  self = super(View, self, init);
  if (self) {
    FlagStatusView *this = (FlagStatusView *) self;

    this->icon = $(alloc(ImageView), initWithFrame, &MakeRect(0, 0, HUD_PIC_HEIGHT, HUD_PIC_HEIGHT));
    assert(this->icon);

    $((View *) this->icon, addClassName, "icon");
    $(self, addSubviewRelativeTo, (View *) this->icon, (View *) ((CounterView *) self)->caption, ViewPositionBefore);
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
  const TeamScoreView *teamScore = (TeamScoreView *) self;

  const char state = FlagStatusView_flagState(teamScore);

  if (state == '\0') {
    this->icon->color.a = 0;
    return;
  }

  if (this->icon->image == NULL) {
    $(this->icon, setImage, (Image *) Cg_HudImage(va("pics/flag%d", teamScore->team)));
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
 * @see TeamScoreView::captionForTeam(const TeamScoreView *, const CGameTeamInfo *)
 */
static const char *FlagStatusView_captionForTeam(const TeamScoreView *self, const CGameTeamInfo *team) {

  switch (FlagStatusView_flagState(self)) {
    case CTF_FLAG_BASE:
      return "Base";
    case CTF_FLAG_CARRIED:
      return "Stolen";
    case CTF_FLAG_DROPPED:
      return "Dropped";
    default:
      return team->name;
  }
}

/**
 * @see Class::initialize(Class *)
 */
static void FlagStatusView_initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = FlagStatusView_dealloc;

  ((ViewInterface *) clazz->interface)->init = FlagStatusView_init;
  ((ViewInterface *) clazz->interface)->updateBindings = FlagStatusView_updateBindings;

  ((TeamScoreViewInterface *) clazz->interface)->captionForTeam = FlagStatusView_captionForTeam;
}

Class *_FlagStatusView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "FlagStatusView",
      .superclass = _TeamScoreView(),
      .instanceSize = sizeof(FlagStatusView),
      .interfaceSize = sizeof(FlagStatusViewInterface),
      .initialize = FlagStatusView_initialize,
    });
  });

  return clazz;
}

#undef _Class

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

  installed = true;
}
