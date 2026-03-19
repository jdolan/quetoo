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

/**
 * @brief Adds an ambient loop sound when the player's view is underwater.
 */
static void Cg_AddUnderwater(void) {

  if (cgi.view->contents & CONTENTS_MASK_LIQUID) {
    Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
      .sample = cg_sample_underwater,
      .flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME,
      .entity = Cg_Self()
    });
  }
}

/**
 * @brief
 */
void Cg_AddEffects(void) {

  Cg_AddUnderwater();
}
