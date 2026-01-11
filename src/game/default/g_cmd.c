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

#include "g_local.h"

/**
 * @brief Give items to a client
 */
static void G_Give_f(g_client_t *cl) {
  const g_item_t *it;
  uint32_t quantity;
  uint32_t i;
  bool give_all;
  g_entity_t *it_ent;

  if (sv_max_clients->integer > 1 && !g_cheats->value) {
    gi.ClientPrint(cl, PRINT_HIGH, "Cheats are disabled\n");
    return;
  }

  const char *name = gi.Args();

  if (gi.Argc() == 3) {
    quantity = (uint32_t) strtol(gi.Argv(2), NULL, 10);

    if (quantity > 9999) {
      quantity = 9999;
    }
  } else {
    quantity = 9999;
  }

  if (g_ascii_strcasecmp(name, "all") == 0) {
    give_all = true;
  } else {
    give_all = false;
  }

  if (give_all || g_ascii_strcasecmp(gi.Argv(1), "health") == 0) {
    if (gi.Argc() == 3) {
      cl->entity->health = quantity;
    } else {
      cl->entity->health = cl->entity->max_health + 5;
    }
    if (!give_all) {
      return;
    }
  }

  if (give_all || g_ascii_strcasecmp(name, "armor") == 0) {
    for (i = 0; i < g_num_items; i++) {
      it = G_ItemByIndex(i);
      if (!it->Pickup) {
        continue;
      }
      if (it->type != ITEM_ARMOR) {
        continue;
      }
      if (!G_ArmorInfo(it)) {
        continue;
      }
      cl->inventory[i] = it->max;
    }
    if (!give_all) {
      return;
    }
  }

  if (give_all || g_ascii_strcasecmp(name, "weapons") == 0) {
    for (i = 0; i < g_num_items; i++) {
      it = G_ItemByIndex(i);
      if (!it->Pickup) {
        continue;
      }
      if (it->type != ITEM_WEAPON) {
        continue;
      }
      cl->inventory[i] += 1;
    }
    if (!give_all) {
      return;
    }
  }

  if (give_all || g_ascii_strcasecmp(name, "ammo") == 0) {
    for (i = 0; i < g_num_items; i++) {
      it = G_ItemByIndex(i);
      if (!it->Pickup) {
        continue;
      }
      if (it->type != ITEM_AMMO) {
        continue;
      }
      G_AddAmmo(cl, it, quantity);
    }
    if (!give_all) {
      return;
    }
  }

  if (give_all) { // we've given full health and inventory
    return;
  }

  it = G_FindItem(name);
  if (!it) {
    name = gi.Argv(1);
    it = G_FindItem(name);
    if (!it) {
      gi.ClientPrint(cl, PRINT_HIGH, "Unknown item: %s\n", name);
      return;
    }
  }

  if (!it->Pickup) {
    gi.ClientPrint(cl, PRINT_HIGH, "Non-pickup item: %s\n", name);
    return;
  }

  if (it->type == ITEM_AMMO) { // give the requested ammo quantity

    if (gi.Argc() == 3) {
      cl->inventory[it->index] = quantity;
    } else {
      cl->inventory[it->index] += it->quantity;
    }
  } else { // or spawn and touch whatever they asked for
    it_ent = G_AllocEntity(it->classname);

    G_SpawnItem(it_ent, it);
    G_TouchItem(it_ent, cl->entity, NULL);

    if (it_ent->in_use) {
      G_FreeEntity(it_ent);
    }
  }
}

/**
 * @brief
 */
static void G_God_f(g_client_t *cl) {
  char *msg;

  if (sv_max_clients->integer > 1 && !g_cheats->value) {
    gi.ClientPrint(cl, PRINT_HIGH, "Cheats are disabled\n");
    return;
  }

  cl->entity->flags ^= FL_GOD_MODE;
  if (!(cl->entity->flags & FL_GOD_MODE)) {
    msg = "god OFF\n";
  } else {
    msg = "god ON\n";
  }

  gi.ClientPrint(cl, PRINT_HIGH, "%s", msg);
}

/**
 * @brief
 */
static void G_NoClip_f(g_client_t *cl) {
  char *msg;

  if (sv_max_clients->integer > 1 && !g_cheats->value) {
    gi.ClientPrint(cl, PRINT_HIGH, "Cheats are disabled\n");
    return;
  }

  if (cl->entity->move_type == MOVE_TYPE_NO_CLIP) {
    cl->entity->move_type = MOVE_TYPE_WALK;
    msg = "no_clip OFF\n";
  } else {
    cl->entity->move_type = MOVE_TYPE_NO_CLIP;
    msg = "no_clip ON\n";
  }

  gi.ClientPrint(cl, PRINT_HIGH, "%s", msg);
}

/**
 * @brief
 */
static void G_Wave_f(g_client_t *cl) {

  if (cl->entity->sv_flags & SVF_NO_CLIENT) {
    return;
  }

  if (cl->entity->dead) {
    return;
  }  

  G_SetAnimation(cl, ANIM_TORSO_GESTURE, true);
}

/**
 * @brief
 */
static void G_Use_f(g_client_t *cl) {

  if (cl->entity->dead) {
    return;
  }

  if (G_Ai_InDeveloperMode()) {
    cl->entity->move_node = true;
    return;
  }

  const char *s = gi.Args();
  const g_item_t *it;
  
  if (s && *s) {
    it = G_FindItem(s);
  } else {
    it = cl->last_pickup;

    if (!it) {
      return;
    }
  }
  if (!it) {
    gi.ClientPrint(cl, PRINT_HIGH, "Unknown item: %s\n", s);
    return;
  }
  if (!it->Use) {
    gi.ClientPrint(cl, PRINT_HIGH, "Item is not usable\n");
    return;
  }

  if (!cl->inventory[it->index]) {
    gi.ClientPrint(cl, PRINT_HIGH, "Out of item: %s\n", s);
    return;
  }

  it->Use(cl, it);
}

/**
 * @brief
 */
static void G_Drop_f(g_client_t *cl) {
  const g_item_t *it;

  // we don't drop in instagib or arena
  if (g_level.gameplay) {
    return;
  }

  if (cl->entity->dead) {
    return;
  }

  const char *s = gi.Args();
  it = NULL;

  if (!g_strcmp0(s, "flag")) {
    G_TossFlag(cl);
    return;
  } else if (!g_strcmp0(s, "tech")) {
    G_TossTech(cl);
    return;
  } else { // or just look up the item
    it = G_FindItem(s);
  }

  if (!it) {
    gi.ClientPrint(cl, PRINT_HIGH, "Unknown item: %s\n", s);
    return;
  }

  if (!it->Drop) {
    gi.ClientPrint(cl, PRINT_HIGH, "Item can not be dropped\n");
    return;
  }

  const uint16_t index = it->index;

  if (cl->inventory[index] == 0) {
    gi.ClientPrint(cl, PRINT_HIGH, "Out of item: %s\n", s);
    return;
  }

  int32_t drop_quantity;

  if (it->type == ITEM_AMMO) {
    drop_quantity = it->quantity;
  } else {
    drop_quantity = 1;
  }

  if (cl->inventory[index] < drop_quantity) {
    gi.ClientPrint(cl, PRINT_HIGH, "Quantity too low: %s\n", s);
    return;
  }

  cl->inventory[index] -= drop_quantity;
  cl->last_dropped = it;

  it->Drop(cl, it);

  // adjust weapon if we need to
  if (it->type == ITEM_WEAPON) {
    if (cl->weapon == it && !cl->next_weapon && !cl->inventory[index]) {
      G_UseBestWeapon(cl);
    }
  }
}

/**
 * @brief
 */
static void G_WeaponLast_f(g_client_t *cl) {

  if (!cl->weapon || !cl->prev_weapon) {
    return;
  }

  const uint16_t index = cl->prev_weapon->index;

  if (!cl->inventory[index]) {
    return;
  }

  const g_item_t *it = G_ItemByIndex(index);

  if (!it->Use) {
    return;
  }

  if (it->type != ITEM_WEAPON) {
    return;
  }

  it->Use(cl, it);
}

/**
 * @brief
 */
static void G_Kill_f(g_client_t *cl) {

  if ((g_level.time - cl->respawn_time) < 1000) {
    return;
  }

  if (cl->persistent.spectator) {
    return;
  }

  if (cl->entity->dead) {
    return;
  }

  cl->entity->flags &= ~FL_GOD_MODE;

  cl->entity->dead = true;
  cl->entity->health = 0;

  cl->entity->Die(cl->entity, cl->entity, MOD_SUICIDE);
}


/**
 * @brief Server console command for muting players by name (toggles)
 */
void G_Mute_f(void) {
  if (gi.Argc() < 2) {
    return;
  }

  g_client_t *cl = G_ClientByName(va("%s", gi.Argv(2)));

  if (!cl) {
    return;
  }

  if (cl->persistent.muted) {
    cl->persistent.muted = false;
    gi.Print(" %s is now unmuted\n", cl->persistent.net_name);
  } else {
    cl->persistent.muted = true;
    gi.Print(" %s is now muted\n", cl->persistent.net_name);
  }
}

/**
 * @brief This is the client-specific sibling to Cvar_VariableString.
 */
static const char *G_ExpandVariable(g_client_t *cl, char v) {
  int32_t i;

  switch (v) {

    case 'd': // last dropped item
      if (cl->last_dropped) {
        return cl->last_dropped->name;
      }
      return "";

    case 'h': // health
      i = cl->ps.stats[STAT_HEALTH];
      return va("%d", i);

    case 'a': // armor
      i = cl->ps.stats[STAT_ARMOR];
      return va("%d", i);

    default:
      return "";
  }
}

/**
 * @brief
 */
static char *G_ExpandVariables(g_client_t *cl, const char *text) {
  static char expanded[MAX_STRING_CHARS];
  size_t i, j, len;

  if (!text || !text[0]) {
    return "";
  }

  memset(expanded, 0, sizeof(expanded));
  len = strlen(text);

  for (i = j = 0; i < len; i++) {
    if (text[i] == '%' && i < len - 1) { // expand %variables
      const char *c = G_ExpandVariable(cl, text[i + 1]);
      strcat(expanded, c);
      j += strlen(c);
      i++;
    } else
      // or just append normal chars
    {
      expanded[j++] = text[i];
    }
  }

  return expanded;
}

/**
 * @brief
 */
static void G_Say_f(g_client_t *cl) {
  char text[MAX_STRING_CHARS];
  char temp[MAX_STRING_CHARS];

  if (cl->persistent.muted) {
    gi.ClientPrint(cl, PRINT_HIGH, "You have been muted\n");
    return;
  }

  text[0] = '\0';

  bool team = false; // whether or not we're dealing with team chat
  bool arg0 = true; // whether or not we need to print arg0

  if (!g_strcmp0(gi.Argv(0), "say") || !g_strcmp0(gi.Argv(0), "say_team")) {
    arg0 = false;

    if (!g_strcmp0(gi.Argv(0), "say_team") && (g_level.teams || g_level.ctf)) {
      team = true;
    }
  }

  // if g_spectator_chat is off, spectators can only chat to other spectators
  // and so we force team-chat on them
  if (cl->persistent.spectator && !g_spectator_chat->integer) {
    team = true;
  }

  char *s;
  if (arg0) { // not say or say_team, just arbitrary chat from the console
    s = G_ExpandVariables(cl, va("%s %s", gi.Argv(0), gi.Args()));
  } else { // say or say_team
    s = G_ExpandVariables(cl, va("%s", gi.Args()));
  }

  // strip quotes
  if (s[0] == '"' && s[strlen(s) - 1] == '"') {
    s[strlen(s) - 1] = '\0';
    s++;
  }

  // suppress empty messages
  StrStrip(s, temp);
  if (!strlen(temp)) {
    return;
  }

  if (!team) { // chat flood protection, does not pertain to teams

    if (g_level.time < cl->chat_time) {
      return;
    }

    cl->chat_time = g_level.time + 250;
  }

  const int32_t color = team ? ESC_COLOR_TEAM_CHAT : ESC_COLOR_CHAT;
  g_snprintf(text, sizeof(text), "%s^%d: %s\n", cl->persistent.net_name, color, s);

  G_ForEachClient(other, {
    if (team) {
      if (!G_OnSameTeam(cl, other)) {
        continue;
      }
      gi.ClientPrint(other, PRINT_TEAM_CHAT, "%s", text);
    } else {
      gi.ClientPrint(other, PRINT_CHAT, "%s", text);
    }
  });

  if (dedicated->value) { // print to the console
    gi.Print("%s", text);
  }
}

/**
 * @brief
 */
static void G_PlayerList_f(g_client_t *cl) {
  char text[MAX_PRINT_MSG] = "";

  // connect time, ping, score, name
  G_ForEachClient(c, {
    const int32_t seconds = (g_level.frame_num - c->persistent.first_frame) / QUETOO_TICK_RATE;

    char st[80];
    g_snprintf(st, sizeof(st), "%02d:%02d %4d %3d %-16s %s\n", (seconds / 60), (seconds % 60),
               c->ping,
               c->persistent.score,
               c->persistent.net_name,
               c->persistent.skin);

    if (strlen(text) + strlen(st) > sizeof(text) - 200) {
      sprintf(text + strlen(text), "And more...\n");
      gi.ClientPrint(cl, PRINT_HIGH, "%s", text);
      return;
    }

    strcat(text, st);
  });

  gi.ClientPrint(cl, PRINT_HIGH, "%s", text);
}

/**
 * @brief Returns true if the client's team was changed, false otherwise.
 */
bool G_AddClientToTeam(g_client_t *cl, const char *team_name) {
  g_team_t *team;

  if (!(team = G_TeamByName(team_name))) { // resolve team
    gi.ClientPrint(cl, PRINT_HIGH, "Team \"%s\" doesn't exist\n", team_name);
    return false;
  }

  if (cl->persistent.team == team) {
    return false;
  }

  if (!cl->persistent.spectator) { // changing teams
    G_TossQuadDamage(cl);
    G_TossFlag(cl);
    G_TossTech(cl);
    G_HookDetach(cl);
  }

  cl->persistent.team = team;
  cl->persistent.spectator = false;

  char *user_info = g_strdup(cl->persistent.user_info);
  G_ClientUserInfoChanged(cl, user_info);
  g_free(user_info);

  return true;
}

/**
 * @brief
 */
static void G_Team_f(g_client_t *cl) {

  if ((g_level.teams || g_level.ctf) && gi.Argc() != 2) {
    gi.ClientPrint(cl, PRINT_HIGH, "Usage: %s <team name>\n", gi.Argv(0));
    return;
  }

  if (!g_level.teams && !g_level.ctf) {
    gi.ClientPrint(cl, PRINT_HIGH, "Teams are disabled\n");
    return;
  }

  if (!G_AddClientToTeam(cl, gi.Argv(1))) {
    return;
  }

  G_ClientRespawn(cl, true);
}

/**
 * @brief
 */
static void G_Spectate_f(g_client_t *cl) {

  // prevent spectator spamming
  if (g_level.time - cl->respawn_time < 1000) {
    return;
  }

  if (!g_strcmp0(gi.Argv(0), "spectate")) {

    if (cl->persistent.spectator) {
      gi.ClientPrint(cl, PRINT_HIGH, "You are already spectating\n");
      return;
    }

    G_TossQuadDamage(cl);
    G_TossFlag(cl);
    G_TossTech(cl);
    G_HookDetach(cl);

    gi.WriteByte(SV_CMD_MUZZLE_FLASH);
    gi.WriteShort(cl->entity->s.number);
    gi.WriteByte(MZ_LOGOUT);
    gi.Multicast(cl->entity->s.origin, MULTICAST_PHS);

  } else if (!g_strcmp0(gi.Argv(0), "join")) {

    if (!cl->persistent.spectator) {
      gi.ClientPrint(cl, PRINT_HIGH, "You have already joined\n");
      return;
    }

    if (g_level.teams || g_level.ctf) {
      if (g_auto_join->value) { // assign them to a team
        G_AddClientToTeam(cl, G_SmallestTeam()->name);
      } else { // or ask them to pick
        gi.ClientPrint(cl, PRINT_HIGH, "Use team <team name> to join the game\n");
        return;
      }
    }
  }

  cl->persistent.spectator = !cl->persistent.spectator;
  G_ClientRespawn(cl, true);
}

/**
 * @brief
 */
static void G_Admin_f(g_client_t *cl) {

  if (strlen(g_admin_password->string) == 0) { // blank password (default) disabled
    gi.ClientPrint(cl, PRINT_HIGH, "Admin features disabled\n");
    return;
  }

  if (gi.Argc() < 2) {  // no arguments supplied, show help
    if (!cl->persistent.admin) {
      gi.ClientPrint(cl, PRINT_HIGH, "Usage: admin <password>\n");
    } else {
      gi.ClientPrint(cl, PRINT_HIGH, "Admin commands:\n");
      gi.ClientPrint(cl, PRINT_HIGH, "kick, remove, mute, unmute, timeout, timein\n");
    }
    return;
  }

  if (!cl->persistent.admin) { // not yet an admin, assuming auth
    if (g_strcmp0(gi.Argv(1), g_admin_password->string) == 0) {
      cl->persistent.admin = true;
      gi.BroadcastPrint(PRINT_HIGH, "%s became an admin\n", cl->persistent.net_name);
    } else {
      gi.ClientPrint(cl, PRINT_HIGH, "Invalid admin password\n");
    }
    return;
  }

  if (gi.Argc() > 2) {
    if (g_strcmp0(gi.Argv(2), "mute") == 0) {
      G_MuteClient(va("%s", gi.Argv(3)), true);
    }
  }
}

#ifdef _DEBUG
void G_RecordPmove(void);
void G_PlayPmove(void);
#endif

/**
 * @brief
 */
void G_ClientCommand(g_client_t *cl) {

  const char *cmd = gi.Argv(0);

  if (g_strcmp0(cmd, "say") == 0) {
    G_Say_f(cl);
    return;
  }
  if (g_strcmp0(cmd, "say_team") == 0) {
    G_Say_f(cl);
    return;
  }

  // most commands can not be executed during intermission
  if (g_level.intermission_time) {
    return;
  }

  // these commands are allowed in a timeout
  if (g_strcmp0(cmd, "admin") == 0) {
    G_Admin_f(cl);
    return;
  }

  // these commands are not allowed during intermission or timeout
  if (g_strcmp0(cmd, "spectate") == 0 || g_strcmp0(cmd, "join") == 0) {
    G_Spectate_f(cl);
  } else if (g_strcmp0(cmd, "team") == 0) {
    G_Team_f(cl);
  } else if (g_strcmp0(cmd, "use") == 0) {
    G_Use_f(cl);
  } else if (g_strcmp0(cmd, "drop") == 0) {
    G_Drop_f(cl);
  } else if (g_strcmp0(cmd, "give") == 0) {
    G_Give_f(cl);
  } else if (g_strcmp0(cmd, "god") == 0) {
    G_God_f(cl);
  } else if (g_strcmp0(cmd, "no_clip") == 0) {
    G_NoClip_f(cl);
  } else if (g_strcmp0(cmd, "wave") == 0) {
    G_Wave_f(cl);
  } else if (g_strcmp0(cmd, "weapon_last") == 0) {
    G_WeaponLast_f(cl);
  } else if (g_strcmp0(cmd, "kill") == 0) {
    G_Kill_f(cl);
  } else if (g_strcmp0(cmd, "player_list") == 0) {
    G_PlayerList_f(cl);
  } else if (g_strcmp0(cmd, "chase_previous") == 0) {
    G_ClientChasePrevious(cl);
  } else if (g_strcmp0(cmd, "chase_next") == 0) {
    G_ClientChaseNext(cl);
  }
#ifdef _DEBUG
  else if (g_strcmp0(cmd, "pmove_record") == 0) {
    G_RecordPmove();
  } else if (g_strcmp0(cmd, "pmove_play") == 0) {
    G_PlayPmove();
  }
#endif

  else
    // anything that doesn't match a command will be a chat
  {
    G_Say_f(cl);
  }
}
