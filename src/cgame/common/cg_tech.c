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

static struct {
  DrawHudElements DrawHudElements;
} super;

/**
 * @brief Draws the tech the client holds, beneath whatever powerups they hold.
 */
static void Cg_DrawHudElements_Tech(const player_state_t *ps, cg_hud_layout_t *layout) {

  super.DrawHudElements(ps, layout);

  const int16_t tech = ps->stats[STAT_TECH];
  if (tech) {
    int32_t ch;
    cgi.BindFont("large", &ch, NULL);

    layout->powerup_y = Cg_DrawPowerup(layout->powerup_y, 0, cg_items[tech].icon, ch);

    cgi.BindFont(NULL, NULL, NULL);
  }
}

/**
 * @brief Installs the tech feature's client side, once per module image.
 * @details See `Cg_Ctf_Init` for why the guard is not optional.
 */
void Cg_Tech_Init(void) {
  static bool installed;

  if (installed) {
    return;
  }

  super.DrawHudElements = Cg_DrawHudElements;
  Cg_DrawHudElements = Cg_DrawHudElements_Tech;

  installed = true;
}
