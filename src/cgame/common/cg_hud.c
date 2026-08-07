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
#include "cg_hud_draw.h"

/**
 * @brief Draws health, ammo and armor numerics and icons.
 */
void Cg_DrawVitals(const player_state_t *ps) {
  int32_t x, cw, ch, x_offset;

  cgi.BindFont("large", &cw, &ch);

  x_offset = 3 * cw;

  if (ps->stats[STAT_HEALTH] > 0) {
    const int16_t health = ps->stats[STAT_HEALTH];

    x = cgi.context->w * 0.5 - x_offset;

    Cg_DrawVital(x, ch, health, Cg_HealthIcon(health), HUD_HEALTH_MED, HUD_HEALTH_LOW);
  }

  if ((cg_state.gameplay & ~GAMEPLAY_TEAMS) != GAMEPLAY_INSTAGIB) {

    const int16_t ammo = Cg_ActiveAmmo(ps);

    if (ammo > 0) {
      const int16_t active = Cg_ActiveWeapon(ps);
      const int16_t ammo_low = active != WEAPON_SELECT_OFF
                                 ? (int16_t) bg_item_defs[cg_weapons[active].ammo_tag].quantity
                                 : 0;
      const r_image_t *ammo_icon = active != WEAPON_SELECT_OFF ? cg_weapons[active].icon : NULL;

      x = cgi.context->w * 0.25 - x_offset;

      Cg_DrawVital(x, ch, ammo, ammo_icon, -1, ammo_low);
    }

    if (ps->stats[STAT_ARMOR] > 0) {
      const int16_t armor = ps->stats[STAT_ARMOR];

      x = cgi.context->w * 0.75 - x_offset;

      Cg_DrawVital(x, ch, armor, Cg_ArmorIcon(ps), HUD_ARMOR_MED, HUD_ARMOR_LOW);
    }
  }

  cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the powerups the client holds, and the time left on each.
 */
int32_t Cg_DrawPowerups(const player_state_t *ps, int32_t y) {
  int32_t ch;

  cgi.BindFont("large", &ch, NULL);

  if (ps->stats[STAT_QUAD_TIME] > 0) {
    y = Cg_DrawPowerup(y, ps->stats[STAT_QUAD_TIME], cg_items[POWERUP_QUAD].icon, ch);
  }

  if (ps->stats[STAT_INVULNERABILITY_TIME] > 0) {
    y = Cg_DrawPowerup(y, ps->stats[STAT_INVULNERABILITY_TIME], cg_items[POWERUP_INVULNERABILITY].icon, ch);
  }

  if (ps->stats[STAT_INVISIBILITY_TIME] > 0) {
    y = Cg_DrawPowerup(y, ps->stats[STAT_INVISIBILITY_TIME], cg_items[POWERUP_INVISIBILITY].icon, ch);
  }

  cgi.BindFont(NULL, NULL, NULL);

  return y;
}

/**
 * @brief Draws the recently picked up item icon and name in the top-right corner.
 */
void Cg_DrawPickup(const player_state_t *ps) {
  int32_t x, y, cw, ch;

  cgi.BindFont(NULL, &cw, &ch);

  const int16_t p = ps->stats[STAT_PICKUP];
  if (p) {
    const int16_t pickup = p & ~STAT_TOGGLE_BIT;

    const char *string = pickup > ITEM_NONE && pickup < ITEM_TOTAL ? bg_item_defs[pickup].name : "";
    const r_image_t *icon = pickup > ITEM_NONE && pickup < ITEM_TOTAL
                              ? cg_items[pickup].icon
                              : NULL;

    x = cgi.context->w - HUD_PIC_HEIGHT - cgi.StringWidth(string);
    y = 0;

    Cg_DrawIcon(x, y, icon, color_white);

    x += HUD_PIC_HEIGHT;
    y += (HUD_PIC_HEIGHT - ch) / 2 + 2;

    cgi.Draw2DString(x, y, string, HUD_COLOR_STAT);
  }
}

/**
 * @brief Draws the player's frag count in the top-right corner of the HUD.
 */
int32_t Cg_DrawFrags(const player_state_t *ps, int32_t y) {
  const int16_t frags = ps->stats[STAT_FRAGS];
  int32_t x, cw, ch;

  cgi.BindFont("small", NULL, &ch);

  if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
    cgi.BindFont(NULL, NULL, NULL);
    return y + HUD_PIC_HEIGHT + ch;
  }

  x = cgi.context->w - cgi.StringWidth("Frags");

  cgi.Draw2DString(x, y, "Frags", color_green);

  cgi.BindFont("large", &cw, NULL);

  x = cgi.context->w - 3 * cw;

  cgi.Draw2DString(x, y + ch, va("%3d", frags), HUD_COLOR_STAT);

  cgi.BindFont(NULL, NULL, NULL);

  return y + HUD_PIC_HEIGHT + ch;
}

/**
 * @brief Draws the player's death count in the top-right corner of the HUD.
 */
int32_t Cg_DrawDeaths(const player_state_t *ps, int32_t y) {
  const int16_t deaths = ps->stats[STAT_DEATHS];
  int32_t x, cw, ch;

  cgi.BindFont("small", NULL, &ch);

  if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
    cgi.BindFont(NULL, NULL, NULL);
    return y + HUD_PIC_HEIGHT + ch;
  }

  x = cgi.context->w - cgi.StringWidth("Deaths");

  cgi.Draw2DString(x, y, "Deaths", color_green);

  cgi.BindFont("large", &cw, NULL);

  x = cgi.context->w - 3 * cw;

  cgi.Draw2DString(x, y + ch, va("%3d", deaths), HUD_COLOR_STAT);

  cgi.BindFont(NULL, NULL, NULL);

  return y + HUD_PIC_HEIGHT + ch;
}

/**
 * @brief Draws the "Spectating" label when the player is a spectator not in chase mode.
 */
void Cg_DrawSpectator(const player_state_t *ps) {
  int32_t x, y, cw;

  if (!ps->stats[STAT_SPECTATOR] || ps->stats[STAT_CHASE]) {
    return;
  }

  cgi.BindFont("small", &cw, NULL);

  x = cgi.context->w - cgi.StringWidth("Spectating");
  y = HUD_PIC_HEIGHT;

  cgi.Draw2DString(x, y, "Spectating", color_green);

  cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the name of the player currently being chased in chasecam mode.
 */
void Cg_DrawChase(const player_state_t *ps) {
  int32_t x, y, ch;
  char string[MAX_INFO_STRING_VALUE * 2], *s;

  // if we've changed chase targets, reset the HUD
  if (ps->stats[STAT_CHASE] != cg_hud_state.chase_target) {
    memset(&cg_hud_state, 0, sizeof(cg_hud_state));
    cg_hud_state.chase_target = ps->stats[STAT_CHASE];
  }

  if (!ps->stats[STAT_CHASE]) {
    return;
  }

  const int32_t e = ps->stats[STAT_CHASE];

  if (e < 0 || e >= MAX_ENTITIES) {
    Cg_Warn("Invalid client info index: %d\n", e);
    return;
  }

  cl_entity_t *ent = cgi.client->entities + e;

  const cg_client_info_t *ci = &cg_state.clients[ent->current.client];

  cgi.BindFont("small", NULL, &ch);

  q_snprintf(string, sizeof(string), "Chasing ^7%s", ci->name);

  if ((s = q_strchr(string, '\\'))) {
    *s = '\0';
  }

  x = cgi.context->w * 0.5 - cgi.StringWidth(string) / 2;
  y = cgi.context->h - HUD_PIC_HEIGHT - ch;

  cgi.Draw2DString(x, y, string, color_green);

  cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the current match time string from the server config string.
 */
int32_t Cg_DrawTime(const player_state_t *ps, int32_t y) {
  int32_t x, ch;
  const char *string = cgi.ConfigString(CS_TIME);

  if (!ps->stats[STAT_TIME]) {
    return y;
  }

  cgi.BindFont("small", NULL, &ch);

  x = cgi.context->w - cgi.StringWidth(string);

  cgi.Draw2DString(x, y, string, color_white);

  cgi.BindFont(NULL, NULL, NULL);

  return y + ch;
}

/**
 * @brief Draws a translucent team-color banner strip at the bottom of the screen.
 */
void Cg_DrawTeamBanner(const player_state_t *ps) {
  const int16_t team = ps->stats[STAT_TEAM];
  int32_t x, y;

  if (team == -1) {
    return;
  }

  const color_t color = ColorHSVA(cg_state.teams[team].hue, 1.f, 1.f, .14f);

  x = 0;
  y = cgi.context->h - 64;

  cgi.Draw2DFill(x, y, cgi.context->w, 64, color);
}

/**
 * @brief Plays the hit sound if the player inflicted damage this frame.
 */
void Cg_DrawDamageInflicted(const player_state_t *ps) {

  if (!cg_hit_sound->integer) {
    return;
  }

  const int16_t dmg = ps->stats[STAT_DAMAGE_INFLICT];
  if (dmg) {

    // play the hit sound
    if (cgi.client->unclamped_time - cg_hud_state.damage.hit_sound_time > 50) {
      cg_hud_state.damage.hit_sound_time = cgi.client->unclamped_time;

      Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
        .sample = dmg >= 25 ? cg_sample_hits[1] : cg_sample_hits[0],
        .entity = Cg_Self()
      });
    }
  }
}

/**
 * @brief Arranges the deathmatch HUD, which is the whole of it for a module that
 * builds no optional feature.
 */
static void Cg_DrawHudElements_Common(const player_state_t *ps, cg_hud_layout_t *layout) {

  Cg_DrawVitals(ps);

  layout->powerup_y = Cg_DrawPowerups(ps, layout->powerup_y);

  Cg_DrawPickup(ps);

  Cg_DrawTeamBanner(ps);

  layout->stat_y = Cg_DrawFrags(ps, layout->stat_y);

  layout->stat_y = Cg_DrawDeaths(ps, layout->stat_y);

  Cg_DrawSpectator(ps);

  Cg_DrawChase(ps);
}

DrawHudElements Cg_DrawHudElements = Cg_DrawHudElements_Common;

/**
 * @brief Draws the HUD for the current frame.
 */
void Cg_DrawHud(const player_state_t *ps) {

  if (!cg_draw_hud->integer) {
    return;
  }

  if (!ps->stats[STAT_TIME]) { // intermission
    return;
  }

  Cg_DrawCrosshair(ps);

  if (editor->value) {
    return;
  }

  int32_t ch;
  cgi.BindFont("small", NULL, &ch);
  cgi.BindFont(NULL, NULL, NULL);

  cg_hud_layout_t layout = {
    .powerup_y = cgi.context->h / 2,
    .stat_y = HUD_PIC_HEIGHT + ch, // the pickup holds the first row
  };

  Cg_DrawHudElements(ps, &layout);

  Cg_DrawTime(ps, layout.stat_y);

  Cg_DrawCenterPrint(ps);

  Cg_DrawTargetName(ps);

  Cg_DrawBlend(ps);

  Cg_DrawDamageInflicted(ps);

  Cg_DrawSelectWeapon(ps);
}

