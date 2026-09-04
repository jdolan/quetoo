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


r_image_t *cg_pickup_blend_image;
r_image_t *cg_quad_blend_image;
r_image_t *cg_invisibility_blend_image;
r_image_t *cg_invulnerability_blend_image;
r_image_t *cg_damage_blend_image;

cvar_t *cg_select_weapon_alpha;
cvar_t *cg_select_weapon_delay;
cvar_t *cg_select_weapon_fade;
cvar_t *cg_select_weapon_interval;

cg_hud_state_t cg_hud_state;

/**
 * @brief Calculate the alpha factor for the specified blend components.
 * @param blend_start_time The start of the blend, in unclamped time.
 * @param blend_decay_time The length of the blend in milliseconds.
 * @param blend_alpha The base alpha value.
 */
static float Cg_CalculateBlendAlpha(const uint32_t blend_start_time, const uint32_t blend_decay_time,
                                    const float blend_alpha) {

  if ((cgi.client->unclamped_time - blend_start_time) <= blend_decay_time) {
    const float time_factor = (float) (cgi.client->unclamped_time - blend_start_time) / blend_decay_time;
    const float alpha = cg_draw_blend->value * (blend_alpha - (time_factor * blend_alpha));

    return alpha;
  }

  return 0.0;
}

#define CG_DAMAGE_BLEND_TIME 1500
#define CG_PICKUP_BLEND_TIME 600

/**
 * @brief Perform composition of the dst/src blends.
 */
static void Cg_AddBlend(color_t *blend, const color_t input) {

  if (input.a <= 0.0) {
    return;
  }

  color_t out = *blend;

  out.a = input.a + out.a * (1.0 - input.a);

  for (int32_t i = 0; i < 3; i++) {
    out.rgba[i] = ((input.rgba[i] * input.a) + ((out.rgba[i] * out.a) * (1.0 - input.a))) / out.a;
  }

  *blend = out;
}

/**
 * @brief Draw a blend flash image with a specified alpha.
 * @param icon The picture to use
 * @param alpha The alpha of the blend
 */
static void Cg_DrawBlendFlashImage(const r_image_t *image, const float alpha) {

  if (alpha <= 0.0) {
    return;
  }

  const color_t color = Color4f(1.0, 1.0, 1.0, alpha);
  cgi.Draw2DImage(0, 0, cgi.context->w, cgi.context->h, image, color);
}

/**
 * @brief Draw a full-screen blend effect based on world interaction.
 */
void Cg_DrawBlend(const player_state_t *ps) {

  if (!cg_draw_blend->value) {
    return;
  }

  color_t blend = color_transparent;
  
  // start with base blend based on view origin conents

  const int32_t contents = cgi.view->contents;

  if ((contents & CONTENTS_MASK_LIQUID) && cg_draw_blend_liquid->value) {
    color_t color;

    const cm_trace_t tr = cgi.Trace(cgi.view->origin, cgi.view->origin, Box3_Zero(), NULL, CONTENTS_MASK_LIQUID);
    if (tr.brush) {
      const char *name = tr.brush->brush_sides[0].material->name;
      color = cgi.LoadMaterial(name, ASSET_CONTEXT_TEXTURES)->color;
      const float f = Maxf(color.r, Maxf(color.g, color.b));
      color = Color_Scale(color, 1.f / f);
    } else {
      if (contents & CONTENTS_LAVA) {
        color = Color4f(.8f, .4f, .1f, 1.f);
      } else if (contents & CONTENTS_SLIME) {
        color = Color4f(.4f, .7f, .2f, 1.f);
      } else {
        color = Color4f(.4f, .5f, .6f, 1.f);
      }
    }

    color.a = Clampf(cg_draw_blend_liquid->value * 0.4, 0.f, 0.4f);

    Cg_AddBlend(&blend, color);
  }

  // pickups

  const int16_t p = ps->stats[STAT_PICKUP] & ~STAT_TOGGLE_BIT;

  if (p && (p != cg_hud_state.blend.pickup)) { // don't flash on same item
    cg_hud_state.blend.pickup_time = cgi.client->unclamped_time;
  }

  cg_hud_state.blend.pickup = p;

  if (cg_hud_state.blend.pickup_time && cg_draw_blend_pickup->value) {
    Cg_DrawBlendFlashImage(cg_pickup_blend_image,
      Cg_CalculateBlendAlpha(cg_hud_state.blend.pickup_time, CG_PICKUP_BLEND_TIME, cg_draw_blend_pickup->value));
  }

  // quad damage powerup

  if (ps->stats[STAT_QUAD_TIME] > 0 && cg_draw_blend_powerup->value) {
    Cg_DrawBlendFlashImage(cg_quad_blend_image,
      fabsf(sinf(Radians(cgi.client->unclamped_time * 0.2))) * cg_draw_blend_powerup->value);
  }

  // invisibility powerup

  if (ps->stats[STAT_INVISIBILITY_TIME] > 0 && cg_draw_blend_powerup->value) {
    Cg_DrawBlendFlashImage(cg_invisibility_blend_image,
      fabsf(sinf(Radians(cgi.client->unclamped_time * 0.2))) * cg_draw_blend_powerup->value);
  }

  // invulnerability powerup

  if (ps->stats[STAT_INVULNERABILITY_TIME] > 0 && cg_draw_blend_powerup->value) {
    Cg_DrawBlendFlashImage(cg_invulnerability_blend_image,
      fabsf(sinf(Radians(cgi.client->unclamped_time * 0.2))) * cg_draw_blend_powerup->value);
  }

  // taken damage

  const int16_t d = ps->stats[STAT_DAMAGE_ARMOR] + ps->stats[STAT_DAMAGE_HEALTH];

  if (d) {
    cg_hud_state.blend.damage_time = cgi.client->unclamped_time;
  }

  if (cg_hud_state.blend.damage_time && cg_draw_blend_damage->value) {
    Cg_DrawBlendFlashImage(cg_damage_blend_image,
      Cg_CalculateBlendAlpha(cg_hud_state.blend.damage_time, CG_DAMAGE_BLEND_TIME, cg_draw_blend_damage->value));
  }

  // if we have a blend, draw it

  if (blend.a > 0.0) {
    cgi.Draw2DFill(0, 0, cgi.context->w, cgi.context->h, blend);
  }
}

/**
 * @brief Parses a center print message from the server into the center print state.
 */
void Cg_ParseCenterPrint(void) {
  char *c, *out, *line;

  memset(&cg_state.center_print, 0, sizeof(cg_state.center_print));

  c = cgi.ReadString();

  line = cg_state.center_print.lines[0];
  out = line;

  while (*c && cg_state.center_print.num_lines < CG_CENTER_PRINT_LINES - 1) {

    if (*c == '\n') {
      line += MAX_STRING_CHARS;
      out = line;
      cg_state.center_print.num_lines++;
      c++;
      continue;
    }

    *out++ = *c++;
  }

  cg_state.center_print.num_lines++;
  cg_state.center_print.time = cgi.client->unclamped_time + 3000;
}

/**
 * @brief Draws the current center print message centered on screen.
 */
void Cg_DrawCenterPrint(const player_state_t *ps) {
  int32_t cw, ch, x, y;
  char *line = cg_state.center_print.lines[0];

  if (ps->stats[STAT_SCORES]) {
    return;
  }

  if (cg_state.center_print.time < cgi.client->unclamped_time) {
    return;
  }

  cgi.BindFont(NULL, &cw, &ch);

  y = (cgi.context->h - cg_state.center_print.num_lines * ch) / 2;

  while (*line) {
    x = (cgi.context->w - cgi.StringWidth(line)) / 2;

    cgi.Draw2DString(x, y, line, color_white);
    line += MAX_STRING_CHARS;
    y += ch;
  }

  cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the name of the player under the crosshair when aimed at a teammate or enemy.
 */
void Cg_DrawTargetName(const player_state_t *ps) {
  static uint32_t time;
  static char name[MAX_INFO_STRING_VALUE];

  if (!cg_draw_target_name->integer) {
    return;
  }

  if (time > cgi.client->unclamped_time) {
    time = 0;
  }

  vec3_t pos = Vec3_Fmaf(cgi.view->origin, MAX_WORLD_DIST, cgi.view->forward);

  const cm_trace_t tr = cgi.Trace(cgi.view->origin, pos, Box3_Zero(), NULL, CONTENTS_MASK_CLIP_PROJECTILE);
  if (tr.fraction < 1.f) {

    const cl_entity_t *ent = tr.ent;
    if (ent->current.model1 == MODEL_CLIENT) {

      const cg_client_info_t *client = &cg_state.clients[ent->current.client];

      q_strlcpy(name, client->name, sizeof(name));
      time = cgi.client->unclamped_time;
    }
  }

  if (cgi.client->unclamped_time - time > 500) {
    *name = '\0';
  }

  if (*name) {
    int32_t ch;
    cgi.BindFont("medium", NULL, &ch);

    const int32_t w = cgi.StringWidth(name);
    const int32_t x = cgi.context->w / 2 - w / 2;
    const int32_t y = cgi.context->h - 192 - ch;

    cgi.Draw2DString(x, y, name, color_green);
  }
}

/**
 * @brief Scrolls the weapon selection bar forward or backward by one weapon slot.
 */
static void Cg_SelectWeapon(const int8_t dir) {
  const player_state_t *ps = &cgi.client->frame.ps;

  if (ps->stats[STAT_SPECTATOR] || ps->pm_state.type == PM_DEAD) {

    if (ps->stats[STAT_CHASE]) {

      if (dir == 1) {
        cgi.Cbuf("chase_next");
      } else {
        cgi.Cbuf("chase_previous");
      }
    }

    return;
  }

  bool has[WEAPON_TOTAL] = { false };
  for (int32_t i = 0; i < WEAPON_TOTAL; i++) {
    has[i] = ps->inventory[WEAPON_FIRST + i] > 0;
  }

  int16_t bit = cg_hud_state.weapon.bit;
  if (bit < 0 || bit >= WEAPON_TOTAL || !has[bit]) {
    const int16_t current_tag = ps->stats[STAT_WEAPON] & 0xFF;
    if (current_tag >= WEAPON_FIRST && current_tag < WEAPON_LAST) {
      bit = current_tag - WEAPON_FIRST;
    } else {
      bit = WEAPON_SELECT_OFF;
    }
  }

  for (int32_t i = 0; i < WEAPON_TOTAL; i++) {

    bit += dir;

    if (bit < 0) {
      bit = WEAPON_TOTAL - 1;
    } else if (bit >= WEAPON_TOTAL) {
      bit = 0;
    }

    if (has[bit]) {
      cg_hud_state.weapon.bit = bit;
      cg_hud_state.weapon.time = cgi.client->unclamped_time + cg_select_weapon_delay->integer;
      cg_hud_state.weapon.bar_time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
      return;
    }
  }

  // should never happen
  cg_hud_state.weapon.bit = WEAPON_SELECT_OFF;
}

/**
 * @brief Ensures the currently selected weapon tag refers to a weapon the player actually carries.
 */
static void Cg_ValidateSelectedWeapon(const player_state_t *ps) {

  // if we were off, start from our current weapon.
  if (cg_hud_state.weapon.bit == WEAPON_SELECT_OFF) {
    cg_hud_state.weapon.bit = Cg_ActiveWeapon(ps);
    return;
  }

  // see if we have this weapon
  if (cg_hud_state.weapon.has[cg_hud_state.weapon.bit]) {
    return; // got it
  }

  // nope, so pick the closest one we have
  for (int32_t i = 2; i < WEAPON_TOTAL * 2; i++) {
    int32_t offset = (int32_t) (((i & 1) ? -i : i) / 2);
    int32_t id = cg_hud_state.weapon.bit + offset;

    if (id < 0 || id >= WEAPON_TOTAL) {
      continue;
    }

    if (cg_hud_state.weapon.has[id]) {
      cg_hud_state.weapon.bit = id;
      return;
    }
  }

  // should never happen
  cg_hud_state.weapon.bit = WEAPON_SELECT_OFF;
}

/**
 * @brief Issues a use command for the pending selected weapon if the selection timer has expired.
 */
bool Cg_AttemptSelectWeapon(const player_state_t *ps) {

  cg_hud_state.weapon.time = 0;

  if (!ps->stats[STAT_SPECTATOR] &&
    cg_hud_state.weapon.bit != -1) {

    if (cg_hud_state.weapon.bit != Cg_ActiveWeapon(ps)) {
      const char *classname = bg_item_defs[cg_weapons[cg_hud_state.weapon.bit].tag].classname;
      cgi.Cbuf(va("use %s\n", classname));

      cg_hud_state.weapon.time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
      cg_hud_state.weapon.bar_time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;

      return true;
    }

    cg_hud_state.weapon.bit = -1;
    return true;
  }

  return false;
}

/**
 * @brief Advances the weapon selection state for the frame.
 * @param ps The player state.
 * @param alpha The weapon bar opacity to return, fading over `cg_select_weapon_fade`.
 * @return Whether the weapon bar is shown.
 */
bool Cg_UpdateSelectWeapon(const player_state_t *ps, float *alpha) {

  *alpha = 0.f;

  // spectator/dead
  if (!Cg_HasWeapon(ps) || ps->pm_state.type == PM_DEAD) {
    cg_hud_state.weapon.bit = -1;
    cg_hud_state.weapon.time = 0;
    cg_hud_state.weapon.bar_time = 0;
    cg_hud_state.weapon.used_bit = 0;
    return false;
  }

  // rebuild weapon availability from inventory every frame
  cg_hud_state.weapon.num = 0;

  for (int32_t i = 0; i < WEAPON_TOTAL; i++) {
    cg_hud_state.weapon.has[i] = ps->inventory[WEAPON_FIRST + i] > 0;

    if (cg_hud_state.weapon.has[i]) {
      cg_hud_state.weapon.num++;
    }
  }

  if (!cg_hud_state.weapon.num) {
    cg_hud_state.weapon.bit = -1;
    cg_hud_state.weapon.time = 0;
    cg_hud_state.weapon.bar_time = 0;
    cg_hud_state.weapon.used_bit = 0;
    return false;
  }

  const int16_t switching = ((ps->stats[STAT_WEAPON] >> 8) & 0xFF);

  if (cg_hud_state.weapon.used_bit != switching) {
    cg_hud_state.weapon.used_bit = switching;

    if (cg_hud_state.weapon.used_bit && !ps->stats[STAT_SPECTATOR]) {

      // we changed weapons without using scrolly, show it for a bit
      cg_hud_state.weapon.bit = cg_hud_state.weapon.used_bit - 1;
      cg_hud_state.weapon.time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
      cg_hud_state.weapon.bar_time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
    }
  }

  // not changing or ran out of time
  if (cg_hud_state.weapon.time <= cgi.client->unclamped_time) {
    Cg_AttemptSelectWeapon(ps);

    if (cg_hud_state.weapon.time <= cgi.client->unclamped_time) {
      return false;
    }
  }

  // figure out weapon.bit
  Cg_ValidateSelectedWeapon(ps);

  if (cg_select_weapon_fade->modified || cg_select_weapon_interval->modified) {
    cg_select_weapon_fade->modified = false;

    cg_select_weapon_fade->value = Clampf(cg_select_weapon_fade->value, 0.f, cg_select_weapon_interval->value);
  }

  const int32_t delta = cg_hud_state.weapon.bar_time - cgi.client->unclamped_time;
  *alpha = Clampf(delta / (float) cg_select_weapon_fade->integer, 0.f, 1.f);

  return true;
}

/**
 * @brief Console command handler to select the previous weapon in the weapon bar.
 */
static void Cg_Weapon_Prev_f(void) {
  Cg_SelectWeapon(-1);
}

/**
 * @brief Console command handler to select the next weapon in the weapon bar.
 */
static void Cg_Weapon_Next_f(void) {
  Cg_SelectWeapon(1);
}

/**
 * @brief Registers HUD console commands and initializes HUD-related console variables.
 */
void Cg_InitHud(void) {
  cgi.AddCmd("cg_weapon_next", Cg_Weapon_Next_f, CMD_CGAME,
         "Open the weapon bar to the next weapon. In chasecam, switches to next target.");
  cgi.AddCmd("cg_weapon_previous", Cg_Weapon_Prev_f, CMD_CGAME,
         "Open the weapon bar to the previous weapon. In chasecam, switches to previous target.");

  cg_select_weapon_alpha = cgi.AddCvar("cg_select_weapon_alpha", "0.5", CVAR_ARCHIVE,
                     "The opacity of unselected weapons in the weapon bar.");
  cg_select_weapon_delay = cgi.AddCvar("cg_select_weapon_delay", "250", CVAR_ARCHIVE,
                     "The amount of time, in milliseconds, to wait between changing weapons in the scroll view.");
  cg_select_weapon_fade = cgi.AddCvar("cg_select_weapon_fade", "200", CVAR_ARCHIVE,
                     "The amount of time, in milliseconds, for the weapon bar to fade in or out.");
  cg_select_weapon_interval = cgi.AddCvar("cg_select_weapon_interval", "750", CVAR_ARCHIVE,
                      "The amount of time, in milliseconds, to show the weapon bar after changing weapons.");
}

/**
 * @brief Loads HUD image assets including the weapon select bar and blend overlay images.
 */
void Cg_LoadHudMedia(void) {
  Cg_InitInventory();

  cg_pickup_blend_image = cgi.LoadImage("pics/bf_pickup", IMG_PIC);
  cg_quad_blend_image = cgi.LoadImage("pics/bf_powerup_quad", IMG_PIC);
  cg_invisibility_blend_image = cgi.LoadImage("pics/bf_powerup_invisibility", IMG_PIC);
  cg_invulnerability_blend_image = cgi.LoadImage("pics/bf_powerup_invulnerability", IMG_PIC);
  cg_damage_blend_image = cgi.LoadImage("pics/bf_damage", IMG_PIC);
}

/**
 * @brief Clear HUD-related state.
 */
void Cg_ClearHud(void) {
  memset(&cg_hud_state, 0, sizeof(cg_hud_state));

  cg_hud_state.weapon.bit = WEAPON_SELECT_OFF;
}
