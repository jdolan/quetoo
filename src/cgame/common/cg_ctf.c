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

#include "ui/hud/HudViewController.h"

#define _Class _HeldFlagView

/**
 * @brief The flag the player carries, pulsing, or hidden.
 * @extends ImageView
 */
typedef struct HeldFlagViewInterface HeldFlagViewInterface;

typedef struct {
  ImageView imageView;
  HeldFlagViewInterface *interface[0];
  g_item_tag_t flag;
} HeldFlagView;

struct HeldFlagViewInterface {
  ImageViewInterface imageViewInterface;
};

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  if (data == NULL) {
    return;
  }

  HeldFlagView *this = (HeldFlagView *) self;
  const player_state_t *ps = &((const cl_frame_t *) data)->ps;

  g_item_tag_t flag = ITEM_NONE;
  for (g_item_tag_t i = FLAG_FIRST; i < FLAG_LAST; i++) {
    if (ps->inventory[i]) {
      flag = i;
      break;
    }
  }

  $(self, setVisibility, flag == ITEM_NONE ? ViewVisibilityHidden : ViewVisibilityVisible);

  if (flag == ITEM_NONE) {
    return;
  }

  if (flag != this->flag) {
    this->flag = flag;
    $((ImageView *) self, setImage, (Image *) Cg_HudImage(bg_item_defs[flag].icon));
  }

  ((ImageView *) self)->color.a = (Uint8) (Clampf(sinf(cgi.client->unclamped_time / 150.f), 0.75f, 1.f) * 255);
}

/**
 * @see View::init(View *)
 */
static View *initHeldFlagView(View *self) {
  return super(View, self, init);
}

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = initHeldFlagView;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

Class *_HeldFlagView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "HeldFlagView",
      .superclass = _ImageView(),
      .instanceSize = sizeof(HeldFlagView),
      .interfaceSize = sizeof(HeldFlagViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class

/**
 * @brief Captures is always team deathmatch: instagib and arena do not apply,
 * and teams are not optional. A single owner, like the game side's
 * `G_ClampGameplay_Ctf`, so it does not add to what `previous` offers.
 * @details Points directly at the `GAMEPLAY_TEAM_DEATHMATCH` row of the shared
 * `g_gameplay_modes` table rather than copying its `name`/`label` into a
 * duplicate row - there is nothing here to drift out of sync with the game
 * side, since it is the same static data.
 */
static const g_gameplay_t *Cg_ListGameplayModes_Ctf(size_t *count) {

  *count = 1;

  for (size_t i = 0; i < lengthof(g_gameplay_modes); i++) {
    if (g_gameplay_modes[i].id == GAMEPLAY_TEAM_DEATHMATCH) {
      return &g_gameplay_modes[i];
    }
  }

  return g_gameplay_modes; // unreachable: GAMEPLAY_TEAM_DEATHMATCH is always in the table
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

  Cg_ListGameplayModes = Cg_ListGameplayModes_Ctf;

  installed = true;
}
