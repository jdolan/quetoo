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
} previous;

/**
 * @brief Draws the flag the client is carrying.
 */
static void Cg_DrawHeldFlag(const player_state_t *ps) {
  int32_t x, y;

  g_item_tag_t flag_tag = ITEM_NONE;

  for (g_item_tag_t i = FLAG_FIRST; i < FLAG_LAST; i++) {
    if (ps->inventory[i]) {
      flag_tag = i;
      break;
    }
  }

  if (flag_tag == ITEM_NONE) {
    return;
  }

  const r_image_t *icon = cg_items[flag_tag].icon;
  if (!icon) {
    return;
  }

  color_t pulse = color_white;
  pulse.a = Clampf(sinf(cgi.client->unclamped_time / 150.0), 0.75f, 1.f);

  x = HUD_PIC_HEIGHT / 2;
  y = cgi.context->h / 2 - HUD_PIC_HEIGHT * 2;

  cgi.Draw2DImage(x, y, icon->width, icon->height, icon, pulse);
}

/**
 * @brief Draws the client's capture count.
 */
static int32_t Cg_DrawCaptures(const player_state_t *ps, int32_t y) {
  const int16_t captures = ps->stats[STAT_CAPTURES];
  int32_t x, cw, ch;

  cgi.BindFont("small", NULL, &ch);

  if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
    cgi.BindFont(NULL, NULL, NULL);
    return y + HUD_PIC_HEIGHT + ch;
  }

  x = cgi.context->w - cgi.StringWidth("Captures");

  cgi.Draw2DString(x, y, "Captures", color_green);

  cgi.BindFont("large", &cw, NULL);

  x = cgi.context->w - 3 * cw;

  cgi.Draw2DString(x, y + ch, va("%3d", captures), HUD_COLOR_STAT);

  cgi.BindFont(NULL, NULL, NULL);

  return y + HUD_PIC_HEIGHT + ch;
}

/**
 * @brief Draws the flag the client carries, and their capture count beneath the
 * stats the deathmatch HUD already drew.
 */
static void Cg_DrawHudElements_Ctf(const player_state_t *ps, cg_hud_layout_t *layout) {

  previous.DrawHudElements(ps, layout);

  Cg_DrawHeldFlag(ps);

  layout->stat_y = Cg_DrawCaptures(ps, layout->stat_y);
}

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

  previous.DrawHudElements = Cg_DrawHudElements;
  Cg_DrawHudElements = Cg_DrawHudElements_Ctf;

  Cg_ListGameplayModes = Cg_ListGameplayModes_Ctf;

  installed = true;
}
