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



cvar_t *cg_chat_lines;
cvar_t *cg_chat_time;
cvar_t *cg_notify_lines;
cvar_t *cg_notify_time;
cvar_t *cg_select_weapon_alpha;
cvar_t *cg_select_weapon_delay;
cvar_t *cg_select_weapon_fade;
cvar_t *cg_select_weapon_interval;

cg_hud_state_t cg_hud_state;


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
  if (cg_select_weapon_fade->integer > 0) {
    *alpha = Clampf(delta / (float) cg_select_weapon_fade->integer, 0.f, 1.f);
  } else {
    *alpha = 1.f;
  }

  return true;
}

/**
 * @brief Opens the chat input, for the team when asked; the ChatView takes it from there.
 */
static void Cg_MessageMode(bool team) {

  cg_hud_state.chat.team = team;

  cgi.SetKeyDest(KEY_CHAT);
}

/**
 * @brief Console command handler to open the chat input.
 */
static void Cg_MessageMode_f(void) {
  Cg_MessageMode(false);
}

/**
 * @brief Console command handler to open the team chat input.
 */
static void Cg_MessageMode2_f(void) {
  Cg_MessageMode(true);
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
  cgi.AddCmd("cg_message_mode", Cg_MessageMode_f, CMD_CGAME, "Open the chat input");
  cgi.AddCmd("cg_message_mode_2", Cg_MessageMode2_f, CMD_CGAME, "Open the team chat input");

  cg_chat_lines = cgi.AddCvar("cg_chat_lines", "4", CVAR_ARCHIVE, "How many chat lines to show on the HUD, 0 disables");
  cg_chat_time = cgi.AddCvar("cg_chat_time", "10.0", CVAR_ARCHIVE, "How long, in seconds, chat lines stay on the HUD");
  cg_notify_lines = cgi.AddCvar("cg_notify_lines", "3", CVAR_ARCHIVE, "How many console lines to show on the HUD, 0 disables");
  cg_notify_time = cgi.AddCvar("cg_notify_time", "3.0", CVAR_ARCHIVE, "How long, in seconds, console lines stay on the HUD");

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

}

/**
 * @brief Clear HUD-related state.
 */
void Cg_ClearHud(void) {
  memset(&cg_hud_state, 0, sizeof(cg_hud_state));

  cg_hud_state.weapon.bit = WEAPON_SELECT_OFF;
  cg_hud_state.clear_time = (uint32_t) SDL_GetTicks();
}
