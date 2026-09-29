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

Cvar *cg_chatLines;
Cvar *cg_chatTime;
Cvar *cg_notifyLines;
Cvar *cg_notifyTime;
Cvar *cg_selectWeaponAlpha;
Cvar *cg_selectWeaponDelay;
Cvar *cg_selectWeaponFade;
Cvar *cg_selectWeaponInterval;

CGameHudState cgameHudState;

/**
 * @brief Parses a center print message from the server into the center print state.
 */
void Cg_ParseCenterPrint(void) {
  char *c, *out, *line;

  memset(&cgameState.centerPrint, 0, sizeof(cgameState.centerPrint));

  c = cgi.ReadString();

  line = cgameState.centerPrint.lines[0];
  out = line;

  while (*c && cgameState.centerPrint.numLines < CG_CENTER_PRINT_LINES - 1) {

    if (*c == '\n') {
      line += MAX_STRING_CHARS;
      out = line;
      cgameState.centerPrint.numLines++;
      c++;
      continue;
    }

    *out++ = *c++;
  }

  cgameState.centerPrint.numLines++;
  cgameState.centerPrint.time = cgi.client->unclampedTime + 3000;
}

/**
 * @brief Scrolls the weapon selection bar forward or backward by one weapon slot.
 */
static void Cg_SelectWeapon(const int8_t dir) {
  const PlayerState *ps = &cgi.client->frame.ps;

  if (cgi.client->demoServer) {
    return; // a demo holds one player: there is nobody to scan to, and the weapon they had
            // selected is theirs rather than the viewer's to change
  }

  if (ps->stats[STAT_SPECTATOR] || ps->pmState.type == PM_DEAD) {

    // not gated on STAT_CHASE: stepping to another target while detached acquires one, which is
    // how a free-flying spectator lands back on a player
    if (dir == 1) {
      cgi.Cbuf("chase_next");
    } else {
      cgi.Cbuf("chase_previous");
    }

    return;
  }

  bool has[WEAPON_TOTAL] = { false };
  for (int32_t i = 0; i < WEAPON_TOTAL; i++) {
    has[i] = ps->inventory[WEAPON_FIRST + i] > 0;
  }

  int16_t bit = cgameHudState.weapon.bit;
  if (bit < 0 || bit >= WEAPON_TOTAL || !has[bit]) {
    const int16_t currentTag = ps->stats[STAT_WEAPON] & 0xFF;
    if (currentTag >= WEAPON_FIRST && currentTag < WEAPON_LAST) {
      bit = currentTag - WEAPON_FIRST;
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
      cgameHudState.weapon.bit = bit;
      cgameHudState.weapon.time = cgi.client->unclampedTime + cg_selectWeaponDelay->integer;
      cgameHudState.weapon.barTime = cgi.client->unclampedTime + cg_selectWeaponInterval->integer;
      return;
    }
  }

  // should never happen
  cgameHudState.weapon.bit = WEAPON_SELECT_OFF;
}

/**
 * @brief Ensures the currently selected weapon tag refers to a weapon the player actually carries.
 */
static void Cg_ValidateSelectedWeapon(const PlayerState *ps) {

  // if we were off, start from our current weapon.
  if (cgameHudState.weapon.bit == WEAPON_SELECT_OFF) {
    cgameHudState.weapon.bit = Cg_ActiveWeapon(ps);
    return;
  }

  // see if we have this weapon
  if (cgameHudState.weapon.has[cgameHudState.weapon.bit]) {
    return; // got it
  }

  // nope, so pick the closest one we have
  for (int32_t i = 2; i < WEAPON_TOTAL * 2; i++) {
    int32_t offset = (int32_t) (((i & 1) ? -i : i) / 2);
    int32_t id = cgameHudState.weapon.bit + offset;

    if (id < 0 || id >= WEAPON_TOTAL) {
      continue;
    }

    if (cgameHudState.weapon.has[id]) {
      cgameHudState.weapon.bit = id;
      return;
    }
  }

  // should never happen
  cgameHudState.weapon.bit = WEAPON_SELECT_OFF;
}

/**
 * @brief Issues a use command for the pending selected weapon if the selection timer has expired.
 */
bool Cg_AttemptSelectWeapon(const PlayerState *ps) {

  cgameHudState.weapon.time = 0;

  if (!ps->stats[STAT_SPECTATOR] &&
    cgameHudState.weapon.bit != -1) {

    if (cgameHudState.weapon.bit != Cg_ActiveWeapon(ps)) {
      const char *classname = bgItemDefs[cgameWeapons[cgameHudState.weapon.bit].tag].classname;
      cgi.Cbuf(va("use %s\n", classname));

      cgameHudState.weapon.time = cgi.client->unclampedTime + cg_selectWeaponInterval->integer;
      cgameHudState.weapon.barTime = cgi.client->unclampedTime + cg_selectWeaponInterval->integer;

      return true;
    }

    cgameHudState.weapon.bit = -1;
    return true;
  }

  return false;
}

/**
 * @brief Advances the weapon selection state for the frame.
 * @param ps The player state.
 * @param alpha The weapon bar opacity to return, fading over `cg_selectWeaponFade`.
 * @return Whether the weapon bar is shown.
 */
bool Cg_UpdateSelectWeapon(const PlayerState *ps, float *alpha) {

  *alpha = 0.f;

  // spectator/dead
  if (!Cg_HasWeapon(ps) || ps->pmState.type == PM_DEAD) {
    cgameHudState.weapon.bit = -1;
    cgameHudState.weapon.time = 0;
    cgameHudState.weapon.barTime = 0;
    cgameHudState.weapon.usedBit = 0;
    return false;
  }

  // rebuild weapon availability from inventory every frame
  cgameHudState.weapon.num = 0;

  for (int32_t i = 0; i < WEAPON_TOTAL; i++) {
    cgameHudState.weapon.has[i] = ps->inventory[WEAPON_FIRST + i] > 0;

    if (cgameHudState.weapon.has[i]) {
      cgameHudState.weapon.num++;
    }
  }

  if (!cgameHudState.weapon.num) {
    cgameHudState.weapon.bit = -1;
    cgameHudState.weapon.time = 0;
    cgameHudState.weapon.barTime = 0;
    cgameHudState.weapon.usedBit = 0;
    return false;
  }

  const int16_t switching = ((ps->stats[STAT_WEAPON] >> 8) & 0xFF);

  if (cgameHudState.weapon.usedBit != switching) {
    cgameHudState.weapon.usedBit = switching;

    if (cgameHudState.weapon.usedBit && !ps->stats[STAT_SPECTATOR]) {

      // we changed weapons without using scrolly, show it for a bit
      cgameHudState.weapon.bit = cgameHudState.weapon.usedBit - 1;
      cgameHudState.weapon.time = cgi.client->unclampedTime + cg_selectWeaponInterval->integer;
      cgameHudState.weapon.barTime = cgi.client->unclampedTime + cg_selectWeaponInterval->integer;
    }
  }

  // not changing or ran out of time
  if (cgameHudState.weapon.time <= cgi.client->unclampedTime) {
    Cg_AttemptSelectWeapon(ps);

    if (cgameHudState.weapon.time <= cgi.client->unclampedTime) {
      return false;
    }
  }

  // figure out weapon.bit
  Cg_ValidateSelectedWeapon(ps);

  if (cg_selectWeaponFade->modified || cg_selectWeaponInterval->modified) {
    cg_selectWeaponFade->modified = false;
    cg_selectWeaponInterval->modified = false;

    cg_selectWeaponFade->value = Clampf(cg_selectWeaponFade->value, 0.f, cg_selectWeaponInterval->value);
  }

  const int32_t delta = cgameHudState.weapon.barTime - cgi.client->unclampedTime;
  if (cg_selectWeaponFade->integer > 0) {
    *alpha = Clampf(delta / (float) cg_selectWeaponFade->integer, 0.f, 1.f);
  } else {
    *alpha = 1.f;
  }

  return true;
}

/**
 * @brief Opens the chat input, for the team when asked; the ChatView takes it from there.
 */
static void Cg_MessageMode(bool team) {

  cgameHudState.chat.team = team;

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
  cgi.AddCmd("cg_weaponNext", Cg_Weapon_Next_f, CMD_CGAME,
         "Open the weapon bar to the next weapon. In chasecam, switches to next target.");
  cgi.AddCmd("cg_weaponPrevious", Cg_Weapon_Prev_f, CMD_CGAME,
         "Open the weapon bar to the previous weapon. In chasecam, switches to previous target.");
  cgi.AddCmd("cg_messageMode", Cg_MessageMode_f, CMD_CGAME, "Open the chat input");
  cgi.AddCmd("cg_messageMode2", Cg_MessageMode2_f, CMD_CGAME, "Open the team chat input");

  cg_chatLines = cgi.AddCvar("cg_chatLines", "4", CVAR_ARCHIVE, "How many chat lines to show on the HUD, 0 disables");
  cg_chatTime = cgi.AddCvar("cg_chatTime", "10.0", CVAR_ARCHIVE, "How long, in seconds, chat lines stay on the HUD");
  cg_notifyLines = cgi.AddCvar("cg_notifyLines", "3", CVAR_ARCHIVE, "How many console lines to show on the HUD, 0 disables");
  cg_notifyTime = cgi.AddCvar("cg_notifyTime", "3.0", CVAR_ARCHIVE, "How long, in seconds, console lines stay on the HUD");

  cg_selectWeaponAlpha = cgi.AddCvar("cg_selectWeaponAlpha", "0.5", CVAR_ARCHIVE,
                     "The opacity of unselected weapons in the weapon bar.");
  cg_selectWeaponDelay = cgi.AddCvar("cg_selectWeaponDelay", "250", CVAR_ARCHIVE,
                     "The amount of time, in milliseconds, to wait between changing weapons in the scroll view.");
  cg_selectWeaponFade = cgi.AddCvar("cg_selectWeaponFade", "200", CVAR_ARCHIVE,
                     "The amount of time, in milliseconds, for the weapon bar to fade in or out.");
  cg_selectWeaponInterval = cgi.AddCvar("cg_selectWeaponInterval", "750", CVAR_ARCHIVE,
                      "The amount of time, in milliseconds, to show the weapon bar after changing weapons.");
}

/**
 * @brief Loads the HUD's per-level media: today, the inventory cache.
 */
void Cg_LoadHudMedia(void) {
  Cg_InitInventory();
}

/**
 * @brief Clear HUD-related state.
 */
void Cg_ClearHud(void) {
  memset(&cgameHudState, 0, sizeof(cgameHudState));

  cgameHudState.weapon.bit = WEAPON_SELECT_OFF;
  cgameHudState.clearTime = (uint32_t) SDL_GetTicks();
}
