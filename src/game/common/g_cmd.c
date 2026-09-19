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
static void G_Give_f(GameClient *cl) {
  const GameItem *it;
  uint32_t quantity;
  bool giveAll;
  GameEntity *itEnt;

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

  if (q_strcasecmp(name, "all") == 0) {
    giveAll = true;
  } else {
    giveAll = false;
  }

  if (giveAll || q_strcasecmp(gi.Argv(1), "health") == 0) {
    if (gi.Argc() == 3) {
      cl->entity->health = quantity;
    } else {
      cl->entity->health = cl->entity->maxHealth + 5;
    }
    if (!giveAll) {
      return;
    }
  }

  if (giveAll || q_strcasecmp(name, "armor") == 0) {
    for (GameItemTag t = ARMOR_FIRST; t < ARMOR_LAST; t++) {
      it = &g_items[t];
      if (!it->Pickup) {
        continue;
      }
      if (!G_ArmorInfo(it)) {
        continue;
      }
      cl->inventory[t] = it->def.max;
    }
    if (!giveAll) {
      return;
    }
  }

  if (giveAll || q_strcasecmp(name, "weapons") == 0) {
    for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
      it = &g_items[t];
      if (!it->Pickup) {
        continue;
      }
      if (!G_ItemAvailable(it)) {
        continue;
      }
      cl->inventory[t] += 1;
    }
    if (!giveAll) {
      return;
    }
  }

  if (giveAll || q_strcasecmp(name, "ammo") == 0) {
    for (GameItemTag t = AMMO_FIRST; t < AMMO_LAST; t++) {
      it = &g_items[t];
      if (!it->Pickup) {
        continue;
      }
      // Give ammo if it's in the active set or used by an entity-promoted weapon.
      bool available = G_ItemAvailable(it);
      if (!available) {
        for (GameItemTag w = WEAPON_FIRST; w < WEAPON_LAST; w++) {
          if (G_ItemAvailable(&g_items[w]) &&
              g_items[w].def.ammo == it->def.tag) {
            available = true;
            break;
          }
        }
      }
      if (!available) {
        continue;
      }
      G_AddAmmo(cl, it, quantity);
    }
    if (!giveAll) {
      return;
    }
  }

  if (giveAll) { // we've given full health and inventory
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

  if (it->def.type == ITEM_TYPE_AMMO) { // give the requested ammo quantity

    if (gi.Argc() == 3) {
      cl->inventory[it->def.tag] = quantity;
    } else {
      cl->inventory[it->def.tag] += it->def.quantity;
    }
  } else { // or spawn and touch whatever they asked for
    itEnt = G_AllocEntity(it->def.classname);

    G_SpawnItem(itEnt, it);
    G_TouchItem(itEnt, cl->entity, NULL);

    if (itEnt->inUse) {
      G_FreeEntity(itEnt);
    }
  }
}

/**
 * @brief Toggles god mode (invulnerability) for the client if cheats are enabled.
 */
static void G_God_f(GameClient *cl) {
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
 * @brief Toggles no-clip movement mode for the client if cheats are enabled.
 */
static void G_NoClip_f(GameClient *cl) {

  if (editor->value) {
    return;
  }

  if (cl->persistent.spectator) {
    return;
  }

  if (sv_max_clients->integer > 1 && !g_cheats->value) {
    gi.ClientPrint(cl, PRINT_HIGH, "Cheats are disabled\n");
  } else if (cl->entity->moveType == MOVE_TYPE_NO_CLIP) {
    cl->entity->moveType = MOVE_TYPE_WALK;
    gi.ClientPrint(cl, PRINT_HIGH, "no_clip disabled\n");
  } else {
    cl->entity->moveType = MOVE_TYPE_NO_CLIP;
    gi.ClientPrint(cl, PRINT_HIGH, "no_clip enabled\n");
  }
}

/**
 * @brief Plays a gesture animation on the client's character model.
 */
static void G_Wave_f(GameClient *cl) {

  if (cl->entity->svFlags & SVF_NO_CLIENT) {
    return;
  }

  if (cl->entity->dead) {
    return;
  }  

  G_SetAnimation(cl, ANIM_TORSO_GESTURE, true);
}

/**
 * @brief Activates an item from the client's inventory by name or last pickup.
 */
static void G_Use_f(GameClient *cl) {

  if (cl->entity->dead) {
    return;
  }

  if (G_Ai_InDeveloperMode()) {
    cl->entity->moveNode = true;
    return;
  }

  const char *s = gi.Args();
  const GameItem *it;
  
  if (s && *s) {
    it = G_FindItem(s);
    if (!it) {
      it = G_FindItemByClassName(s);
    }
  } else {
    it = cl->lastPickup;

    if (!it) {
      return;
    }
  }
  if (!it) {
    gi.ClientPrint(cl, PRINT_HIGH, "Unknown item: %s\n", s);
    return;
  }

  // In Quake item set maps, redirect Quetoo weapon names to their Quake equivalents
  // so that generic bindings (e.g. "use Rocket Launcher") work across both item sets.
  if (g_level.items == ITEMS_QUAKE && it->def.type == ITEM_TYPE_WEAPON) {
    const GameItem *mapped = G_MappedWeapon(it);
    if (mapped) {
      it = mapped;
    }
  }

  if (!it->Use) {
    gi.ClientPrint(cl, PRINT_HIGH, "Item is not usable\n");
    return;
  }

  if (!cl->inventory[it->def.tag]) {
    gi.ClientPrint(cl, PRINT_HIGH, "Out of item: %s\n", s);
    return;
  }

  it->Use(cl, it);
}

/**
 * @brief Drops an item from the client's inventory to the ground.
 */
static void G_Drop_f(GameClient *cl) {

  const char *name = gi.Args();

  const GameItem *it = G_ResolveInventoryItem(cl, name);

  if (!it) {
    gi.ClientPrint(cl, PRINT_HIGH, "Unknown item: %s\n", name);
    return;
  }

  G_DropInventoryItem(cl, it);
}

/**
 * @brief Switches the client back to their previously held weapon.
 */
static void G_WeaponLast_f(GameClient *cl) {

  if (!cl->weapon || !cl->prevWeapon) {
    return;
  }

  const GameItemTag index = cl->prevWeapon->def.tag;

  if (!cl->inventory[index]) {
    return;
  }

  const GameItem *it = &g_items[index];

  if (!it->Use) {
    return;
  }

  if (it->def.type != ITEM_TYPE_WEAPON) {
    return;
  }

  it->Use(cl, it);
}

/**
 * @brief Kills the client via suicide, respecting rate limiting and spectator state.
 */
static void G_Kill_f(GameClient *cl) {

  if ((g_level.time - cl->respawnTime) < 1000) {
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
 * @details Silences the player in voice as well as in chat, for everyone. Muting somebody should
 * take one command, not one per medium, or the operator ends up chasing the same griefer twice.
 */
void G_Mute_f(void) {
  if (gi.Argc() < 2) {
    return;
  }

  GameClient *cl = G_ClientByName(va("%s", gi.Argv(2)));

  if (!cl) {
    return;
  }

  G_SetClientMuted(cl, !cl->persistent.muted);

  gi.Print(" %s is now %smuted\n", cl->persistent.netName, cl->persistent.muted ? "" : "un");
}

/**
 * @brief This is the client-specific sibling to `Cvar_VariableString`.
 */
static const char *G_ExpandVariable(GameClient *cl, char v) {
  int32_t i;

  switch (v) {

    case 'd': // last dropped item
      if (cl->lastDropped) {
        return cl->lastDropped->def.name;
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
 * @brief Expands percent-prefixed variable tokens in the given text string.
 */
static char *G_ExpandVariables(GameClient *cl, const char *text) {
  static char expanded[MAX_STRING_CHARS];
  size_t i, j, len;

  if (!text || !text[0]) {
    return "";
  }

  memset(expanded, 0, sizeof(expanded));
  len = q_strlen(text);

  for (i = j = 0; i < len && j < sizeof(expanded); i++) {
    if (text[i] == '%' && i < len - 1) { // expand %variables
      const char *c = G_ExpandVariable(cl, text[i + 1]);
      q_strlcat(expanded, c, sizeof(expanded));
      j += q_strlen(c);
      i++;
    } else { // or just append normal chars
      expanded[j++] = text[i];
    }
  }

  return expanded;
}

/**
 * @brief Handles the say and `say_team` chat commands, broadcasting text to other clients.
 */
static void G_Say_f(GameClient *cl) {
  char text[MAX_STRING_CHARS];
  char temp[MAX_STRING_CHARS];

  if (cl->persistent.muted) {
    gi.ClientPrint(cl, PRINT_HIGH, "You have been muted\n");
    return;
  }

  text[0] = '\0';

  bool team = false; // whether or not we're dealing with team chat
  bool arg0 = true; // whether or not we need to print arg0

  if (!q_strcmp(gi.Argv(0), "say") || !q_strcmp(gi.Argv(0), "say_team")) {
    arg0 = false;

    if (!q_strcmp(gi.Argv(0), "say_team") && g_level.teams) {
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
  if (s[0] == '"' && s[q_strlen(s) - 1] == '"') {
    s[q_strlen(s) - 1] = '\0';
    s++;
  }

  // suppress empty messages
  q_strcolorstrip(s, temp);
  if (!q_strlen(temp)) {
    return;
  }

  if (!team) { // chat flood protection, does not pertain to teams

    if (g_level.time < cl->chatTime) {
      return;
    }

    cl->chatTime = g_level.time + 250;
  }

  char message[MAX_STRING_CHARS];
  q_strlcpy(message, s, sizeof(message));

  if (!G_ClientWillChat(cl, message, sizeof(message), team)) {
    return;
  }

  const int32_t color = team ? ESC_COLOR_TEAM_CHAT : ESC_COLOR_CHAT;
  q_snprintf(text, sizeof(text), "%s^%d: %s\n", cl->persistent.netName, color, message);

  // chat carries its sender rather than arriving pre-formatted, so the client game decides how it
  // reads, and can attribute it to a player rather than matching text against a pattern
  G_ForEachClient(other, {
    if (other->persistent.mutedClients & ((uint64_t) 1 << cl->ps.client)) {
      continue;
    }
    if (team && !G_OnSameTeam(cl, other)) {
      continue;
    }

    gi.WriteByte(SV_CMD_CHAT);
    gi.WriteByte(cl->ps.client);
    gi.WriteByte(team ? CHAT_TEAM : 0);
    gi.WriteString(message);
    gi.Unicast(other, true);
  });

  if (dedicated->value) { // print to the console
    gi.Print("%s", text);
  }

  G_ClientDidChat(cl, text, team);
}

/**
 * @brief Prints a formatted list of connected players to the requesting client.
 */
static void G_PlayerList_f(GameClient *cl) {
  char text[MAX_PRINT_MSG] = "";

  // connect time, ping, score, name
  G_ForEachClient(c, {
    const int32_t seconds = (g_level.frameNum - c->persistent.firstFrame) / QUETOO_TICK_RATE;

    char st[80];
    q_snprintf(st, sizeof(st), "%02d:%02d %4d %3d %-16s %s\n", (seconds / 60), (seconds % 60),
               c->ping,
               c->persistent.score,
               c->persistent.netName,
               c->persistent.skin);

    if (q_strlen(text) + q_strlen(st) > sizeof(text) - 200) {
      sprintf(text + q_strlen(text), "And more...\n");
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
bool G_AddClientToTeam(GameClient *cl, const char *teamName) {
  GameTeam *team;

  if (!(team = G_TeamByName(teamName))) { // resolve team
    gi.ClientPrint(cl, PRINT_HIGH, "Team \"%s\" doesn't exist\n", teamName);
    return false;
  }

  if (cl->persistent.team == team) {
    return false;
  }

  if (!cl->persistent.spectator) { // changing teams
    G_TossInventory(cl);
  }

  cl->persistent.team = team;
  cl->persistent.spectator = false;

  char *userInfo = q_strdup(cl->persistent.userInfo);
  G_ClientUserInfoChanged(cl, userInfo);
  free(userInfo);

  return true;
}

/**
 * @brief Handles the team command, assigning the client to the named team.
 */
static void G_Team_f(GameClient *cl) {

  if (g_level.teams && gi.Argc() != 2) {
    gi.ClientPrint(cl, PRINT_HIGH, "Usage: %s <team name>\n", gi.Argv(0));
    return;
  }

  if (!g_level.teams) {
    gi.ClientPrint(cl, PRINT_HIGH, "Teams are disabled\n");
    return;
  }

  if (!G_AddClientToTeam(cl, gi.Argv(1))) {
    return;
  }

  G_ClientRespawn(cl, true);
}

/**
 * @brief Handles the spectate and join commands to toggle a client's spectator state.
 */
static void G_Spectate_f(GameClient *cl) {

  // prevent spectator spamming
  if (g_level.time - cl->respawnTime < 1000) {
    return;
  }

  if (!q_strcmp(gi.Argv(0), "spectate")) {

    if (cl->persistent.spectator) {
      gi.ClientPrint(cl, PRINT_HIGH, "You are already spectating\n");
      return;
    }

    G_TossInventory(cl);

    gi.WriteByte(SV_CMD_MUZZLE_FLASH);
    gi.WriteShort(cl->entity->s.number);
    gi.WriteByte(MZ_LOGOUT);
    gi.Multicast(cl->entity->s.origin, MULTICAST_PHS);

  } else if (!q_strcmp(gi.Argv(0), "join")) {

    if (!cl->persistent.spectator) {
      gi.ClientPrint(cl, PRINT_HIGH, "You have already joined\n");
      return;
    }

    if (g_level.teams) {
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
 * @brief Handles the admin command for server administration and privilege escalation.
 */
static void G_Admin_f(GameClient *cl) {

  if (q_strlen(g_admin_password->string) == 0) { // blank password (default) disabled
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
    if (q_strcmp(gi.Argv(1), g_admin_password->string) == 0) {
      cl->persistent.admin = true;
      gi.BroadcastPrint(PRINT_HIGH, "%s became an admin\n", cl->persistent.netName);
    } else {
      gi.ClientPrint(cl, PRINT_HIGH, "Invalid admin password\n");
    }
    return;
  }

  if (gi.Argc() > 2) {
    if (q_strcmp(gi.Argv(2), "mute") == 0) {
      G_MuteClient(va("%s", gi.Argv(3)), true);
    }
  }
}

/**
 * @brief Fires the specified entity on behalf of the editor client, so that movers
 * can be previewed without a functioning trigger.
 */
static void G_EditorUse_f(GameClient *cl) {

  if (!editor->value) {
    return;
  }

  if (gi.Argc() != 2) {
    gi.ClientPrint(cl, PRINT_HIGH, "Usage: editor_use <entity>\n");
    return;
  }

  const int32_t number = atoi(gi.Argv(1));

  if (number < 0 || number >= sv_max_entities->integer) {
    gi.ClientPrint(cl, PRINT_HIGH, "Invalid entity %d\n", number);
    return;
  }

  GameEntity *ent = ge.entities[number];

  if (!ent->inUse) {
    gi.ClientPrint(cl, PRINT_HIGH, "Entity %d is not in use\n", number);
    return;
  }

  if (ent->Use) {
    ent->Use(ent, cl->entity, cl->entity);
  } else {
    G_UseTargets(ent, cl->entity);
  }
}

#if defined(_DEBUG)
void G_RecordPmove(void);
void G_PlayPmove(void);
#endif

/**
 * @brief The tail of the `G_HandleClientCommand` chain, which handles nothing.
 */
static bool G_HandleClientCommand_Common(GameClient *cl, const char *cmd) {
  return false;
}

HandleClientCommand G_HandleClientCommand = G_HandleClientCommand_Common;

/**
 * @brief The tail of the `G_ClientWillChat` chain: everyone may speak.
 */
static bool G_ClientWillChat_Common(GameClient *cl, char *text, size_t size, bool team) {
  return true;
}

ClientWillChat G_ClientWillChat = G_ClientWillChat_Common;

/**
 * @brief The tail of the `G_ClientDidChat` chain: a notification, so it does nothing.
 */
static void G_ClientDidChat_Common(GameClient *cl, const char *text, bool team) {
}

ClientDidChat G_ClientDidChat = G_ClientDidChat_Common;

/**
 * @brief Dispatches an incoming client command string to the appropriate handler.
 */
/**
 * @brief Mutes or unmutes another player for the issuing client.
 * @details The server filters at the source, so a muted player's voice is never relayed here at
 * all. The mute lasts as long as the connection: client numbers are reused, so carrying it further
 * would mean silencing whoever inherits the slot.
 */
static void G_MutePlayer_f(GameClient *cl, bool mute) {

  if (gi.Argc() < 2) {
    gi.ClientPrint(cl, PRINT_HIGH, "Usage: %s <player>\n", gi.Argv(0));
    return;
  }

  GameClient *other = G_ClientByName(va("%s", gi.Argv(1)));

  if (!other) {
    gi.ClientPrint(cl, PRINT_HIGH, "Player \"%s\" not found\n", gi.Argv(1));
    return;
  }

  if (other == cl) {
    gi.ClientPrint(cl, PRINT_HIGH, "You can not mute yourself\n");
    return;
  }

  const uint64_t bit = (uint64_t) 1 << other->ps.client;

  if (mute) {
    cl->persistent.mutedClients |= bit;
  } else {
    cl->persistent.mutedClients &= ~bit;
  }

  gi.MuteVoice(cl, other, mute);

  gi.ClientPrint(cl, PRINT_HIGH, "%s %s\n", other->persistent.netName, mute ? "muted" : "unmuted");
}

void G_ClientCommand(GameClient *cl) {

  const char *cmd = gi.Argv(0);

  if (G_HandleClientCommand(cl, cmd)) {
    return;
  }

  if (q_strcmp(cmd, "mute") == 0) {
    G_MutePlayer_f(cl, true);
    return;
  }
  if (q_strcmp(cmd, "unmute") == 0) {
    G_MutePlayer_f(cl, false);
    return;
  }

  if (q_strcmp(cmd, "say") == 0) {
    G_Say_f(cl);
    return;
  }
  if (q_strcmp(cmd, "say_team") == 0) {
    G_Say_f(cl);
    return;
  }

  // most commands can not be executed during intermission
  if (g_level.intermissionTime) {
    return;
  }

  // these commands are allowed in a timeout
  if (q_strcmp(cmd, "admin") == 0) {
    G_Admin_f(cl);
    return;
  }

  // these commands are not allowed during intermission or timeout
  if (q_strcmp(cmd, "spectate") == 0 || q_strcmp(cmd, "join") == 0) {
    G_Spectate_f(cl);
  } else if (q_strcmp(cmd, "team") == 0) {
    G_Team_f(cl);
  } else if (q_strcmp(cmd, "use") == 0) {
    G_Use_f(cl);
  } else if (q_strcmp(cmd, "drop") == 0) {
    G_Drop_f(cl);
  } else if (q_strcmp(cmd, "give") == 0) {
    G_Give_f(cl);
  } else if (q_strcmp(cmd, "god") == 0) {
    G_God_f(cl);
  } else if (q_strcmp(cmd, "no_clip") == 0) {
    G_NoClip_f(cl);
  } else if (q_strcmp(cmd, "wave") == 0) {
    G_Wave_f(cl);
  } else if (q_strcmp(cmd, "weapon_last") == 0) {
    G_WeaponLast_f(cl);
  } else if (q_strcmp(cmd, "kill") == 0) {
    G_Kill_f(cl);
  } else if (q_strcmp(cmd, "player_list") == 0) {
    G_PlayerList_f(cl);
  } else if (q_strcmp(cmd, "chase_previous") == 0) {
    G_ClientChasePrevious(cl);
  } else if (q_strcmp(cmd, "chase_next") == 0) {
    G_ClientChaseNext(cl);
  } else if (q_strcmp(cmd, "chase_stop") == 0) {
    if (cl->persistent.spectator) {
      G_ClientChaseStop(cl);
    }
  } else if (q_strcmp(cmd, "editor_use") == 0) {
    G_EditorUse_f(cl);
  }
#if defined(_DEBUG)
  else if (q_strcmp(cmd, "pmove_record") == 0) {
    G_RecordPmove();
  } else if (q_strcmp(cmd, "pmove_play") == 0) {
    G_PlayPmove();
  }
#endif

  else
    // anything that doesn't match a command will be a chat
  {
    G_Say_f(cl);
  }
}
