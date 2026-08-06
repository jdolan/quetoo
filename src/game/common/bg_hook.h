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

#pragma once

#include "shared/shared.h"

/**
 * @brief The grappling hook is an optional feature. A game module opts in by
 * adding g_hook.c to its _SOURCES and -DG_HOOK to its AM_CPPFLAGS, then
 * embedding `g_client_hook_t` in its `g_client_t` and `g_hook_style_t` in its
 * `g_client_persistent_t`. The MSVS build sets QuetooGameHook and G_HOOK; the
 * Xcode build adds the source to the target and G_HOOK to its defines.
 *
 * @details This is `bg_` rather than `g_` because both sides use it: the game
 * parses the style a client asked for, and the client game offers the choice.
 * It is deliberately free of game types, both so that `g_types.h` may include it
 * before defining them - `g_hook.h` cannot, because its own declarations need
 * `g_client_t` - and so that the client game may include it at all. The hook owns
 * everything else it needs: its cvars, its media indices and its enabled state
 * all live in g_hook.c. The module supplies only this per-client state, plus the
 * MOD_HOOK, TE_HOOK_IMPACT and TRAIL_HOOK values, which are read off the wire by
 * the client game and so must be numbered by the module.
 *
 * There is no bg_hook.c, unlike bg_pmove and bg_item, because the two functions
 * below are all the shared code there is; both sides still compile them.
 */

struct g_entity_s;

/**
 * @brief Hook style.
 */
typedef enum {
  HOOK_PULL,
  HOOK_SWING_MANUAL,
  HOOK_SWING_AUTO
} g_hook_style_t;

/**
 * @brief Per-client hook state. This is reset when the client respawns; the
 * client's chosen style lives in their persistent state and outlives it.
 */
typedef struct {
  /**
   * @brief Level time when the hook think was last called.
   */
  uint32_t think_time;

  /**
   * @brief Hook may fire again when time exceeds this.
   */
  uint32_t fire_time;

  /**
   * @brief The hook entity the client is attached to.
   */
  struct g_entity_s *entity;

  /**
   * @brief True if the client is currently pulling toward the hook.
   */
  bool pull;
} g_client_hook_t;

/**
 * @return The name of the given hook style, as the `g_hook_style` cvar and a
 * client's `hook_style` user info spell it.
 * @details The game parses these and the client game offers them, so they are
 * declared once here rather than spelled out in a menu and a parser that must
 * agree by inspection.
 */
static inline const char *Hook_StyleName(g_hook_style_t style) {

  switch (style) {
    case HOOK_SWING_MANUAL:
      return "swing_manual";
    case HOOK_SWING_AUTO:
      return "swing_auto";
    default:
      return "pull";
  }
}

/**
 * @return The hook style of the given name, defaulting to `HOOK_PULL` for any
 * name that is not one of them, including "default".
 */
static inline g_hook_style_t Hook_StyleByName(const char *name) {

  for (g_hook_style_t style = HOOK_PULL; style <= HOOK_SWING_AUTO; style++) {
    if (!q_strcmp(name, Hook_StyleName(style))) {
      return style;
    }
  }

  return HOOK_PULL;
}
