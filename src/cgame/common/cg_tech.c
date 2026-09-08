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

#include "ui/hud/PowerupView.h"

#define _Class _TechView

/**
 * @see View::init(View *)
 */
static View *initTechView(View *self) {
  return (View *) $((PowerupView *) self, initWithPowerup, PowerupViewNone);
}

/**
 * @brief The held tech in the powerup column: its icon, with no countdown.
 * @extends PowerupView
 */
typedef struct TechViewInterface TechViewInterface;

typedef struct {
  PowerupView powerupView;
  TechViewInterface *interface[0];
} TechView;

struct TechViewInterface {
  PowerupViewInterface powerupViewInterface;
};

static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  if (data) {
    const player_state_t *ps = &((const cl_frame_t *) data)->ps;
    $((PowerupView *) self, update, ps->stats[STAT_TECH], -1);
  }
}

static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = initTechView;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

Class *_TechView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "TechView",
      .superclass = _PowerupView(),
      .instanceSize = sizeof(TechView),
      .interfaceSize = sizeof(TechViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class

/**
 * @brief Installs the tech feature's client side, once per module image.
 * @details See `Cg_Ctf_Init` for why the guard is not optional.
 */
void Cg_Tech_Init(void) {
  static bool installed;

  if (installed) {
    return;
  }

  installed = true;
}
