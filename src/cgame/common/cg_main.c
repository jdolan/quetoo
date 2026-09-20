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

CGameState cgState;

Cvar *cg_addAtmospheric;
Cvar *cg_addDecals;
Cvar *cg_addEntities;
Cvar *cg_addFlares;
Cvar *cg_addLights;
Cvar *cg_addSprites;
Cvar *cg_addWeather;
Cvar *cg_bob;
Cvar *cg_drawBlend;
Cvar *cg_drawBlendDamage;
Cvar *cg_drawBlendLiquid;
Cvar *cg_drawBlendPickup;
Cvar *cg_drawBlendPowerup;
Cvar *cg_drawCrosshair;
Cvar *cg_drawCrosshairAlpha;
Cvar *cg_drawCrosshairColor;
Cvar *cg_drawCrosshairHealth;
Cvar *cg_drawCrosshairPulse;
Cvar *cg_drawCrosshairScale;
Cvar *cg_drawDiagnostics;
Cvar *cg_drawFps;
Cvar *cg_drawHud;
Cvar *cg_drawPing;
Cvar *cg_drawPingWarn;
Cvar *cg_hud;
Cvar *cg_drawTargetName;
Cvar *cg_drawWeapon;
Cvar *cg_drawWeaponAlpha;
Cvar *cg_drawWeaponBob;
Cvar *cg_drawWeaponX;
Cvar *cg_drawWeaponY;
Cvar *cg_drawWeaponZ;
Cvar *cg_drawVitalsPulse;
Cvar *cg_entityBob;
Cvar *cg_entityRotate;
Cvar *cg_forceSkin;
Cvar *cg_fov;
Cvar *cg_fovZoom;
Cvar *cg_fovInterpolate;
Cvar *cg_hitSound;
Cvar *cg_predict;
Cvar *cg_quickJoinMaxPing;
Cvar *cg_quickJoinMinClients;
Cvar *cg_spritePhysics;
Cvar *cg_cameraMode;
Cvar *cg_thirdPerson;
Cvar *cg_thirdPersonX;
Cvar *cg_thirdPersonY;
Cvar *cg_thirdPersonZ;
Cvar *cg_thirdPersonPitch;
Cvar *cg_thirdPersonYaw;

Cvar *cg_autoSwitch;
Cvar *cg_color;
Cvar *cg_hand;
Cvar *cg_helmet;
#if defined(G_HOOK)
Cvar *cg_hookStyle;
#endif
Cvar *cg_pants;
Cvar *cg_shirt;
Cvar *cg_skin;

Cvar *editor;

CGameImport cgi;
static CGameExport cge;

/**
 * @brief Called when the client first comes up or switches game directories. Client
 * game modules should bootstrap any globals they require here.
 */
static void Cg_Init(void) {

  cgi.Print("Client game module initialization...\n");

  const char *s = va("%s %s", VERSION, BUILD);
  Cvar *cgameVersion = cgi.AddCvar("cgameVersion", s, CVAR_NO_SET, NULL);

  cgi.Print("  Version:    ^2%s^7\n", cgameVersion->string);

  Cg_InitInput();

  cg_addAtmospheric = cgi.AddCvar("cg_addAtmospheric", "1", CVAR_ARCHIVE, "Controls the intensity of atmospheric effects.");
  cg_addDecals = cgi.AddCvar("cg_addDecals", "1", CVAR_ARCHIVE, "Controls decals (bullet holes, blood, etc.).");
  cg_addEntities = cgi.AddCvar("cg_addEntities", "1", 0, "Toggles adding entities to the scene.");
  cg_addFlares = cgi.AddCvar("cg_addFlares", "1", CVAR_ARCHIVE, "Toggles adding flare effects to light sources.");
  cg_addLights = cgi.AddCvar("cg_addLights", "1", 0, "Toggles adding dynamic lights to the scene.");
  cg_addSprites = cgi.AddCvar("cg_addSprites", "1", 0, "Toggles adding sprites to the scene.");
  cg_addWeather = cgi.AddCvar("cg_addWeather", "1", CVAR_ARCHIVE, "Controls the intensity of weather effects.");
  cg_bob = cgi.AddCvar("cg_bob", "1", CVAR_ARCHIVE, "Controls weapon bobbing effect.");
  cg_drawBlend = cgi.AddCvar("cg_drawBlend", "1", CVAR_ARCHIVE, "Controls the intensity of screen alpha-blending.");
  cg_drawBlendDamage = cgi.AddCvar("cg_drawBlendDamage", "1", CVAR_ARCHIVE, "Controls the intensity of the blend flash effect when taking damage.");
  cg_drawBlendLiquid = cgi.AddCvar("cg_drawBlendLiquid", "1", CVAR_ARCHIVE, "Controls the intensity of the blend effect while in a liquid.");
  cg_drawBlendPickup = cgi.AddCvar("cg_drawBlendPickup", "1", CVAR_ARCHIVE, "Controls the intensity of the blend flash effect when picking up items.");
  cg_drawBlendPowerup = cgi.AddCvar("cg_drawBlendPowerup", "1", CVAR_ARCHIVE, "Controls the intensity of the blend flash effect when holding a powerup.");
  cg_drawCrosshair = cgi.AddCvar("cg_drawCrosshair", "1", CVAR_ARCHIVE, "Which crosshair image to use, 0 disables (Default is 1)");
  cg_drawCrosshairAlpha = cgi.AddCvar("cg_drawCrosshairAlpha", "1.0", CVAR_ARCHIVE, "Opacity of the crosshair");
  cg_drawCrosshairColor = cgi.AddCvar("cg_drawCrosshairColor", "default", CVAR_ARCHIVE, "Specifies your crosshair color, in the hex format \"rrggbb\". \"default\" uses white.");
  cg_drawCrosshairHealth = cgi.AddCvar("cg_drawCrosshairHealth", "0", CVAR_ARCHIVE, "Method of coloring the crosshair by health. Range from 1-5, 0 disables.");
  cg_drawCrosshairPulse = cgi.AddCvar("cg_drawCrosshairPulse", "1", CVAR_ARCHIVE, "Pulse the crosshair when picking up items");
  cg_drawCrosshairScale = cgi.AddCvar("cg_drawCrosshairScale", "1", CVAR_ARCHIVE, "Controls the crosshair scale (size)");
  cg_drawDiagnostics = cgi.AddCvar("cg_drawDiagnostics", "0", CVAR_ARCHIVE, "Draw the client, renderer and sound counters on the HUD");
  cg_drawFps = cgi.AddCvar("cg_drawFps", "1", CVAR_ARCHIVE, "Draw the frame rate on the HUD");
  cg_drawHud = cgi.AddCvar("cg_drawHud", "1", CVAR_ARCHIVE, "Render the Heads-Up-Display");
  cg_drawPing = cgi.AddCvar("cg_drawPing", "1", CVAR_ARCHIVE, "Draw the round trip time to the server on the HUD");
  cg_drawPingWarn = cgi.AddCvar("cg_drawPingWarn", "200", CVAR_ARCHIVE, "The round trip time, in milliseconds, above which the ping is drawn as lagging");
  cg_hud = cgi.AddCvar("cg_hud", "default", CVAR_ARCHIVE, "The HUD variant: the ui/hud/<name> directory its layout and style are read from (Default is default)");
  cg_drawTargetName = cgi.AddCvar("cg_drawTargetName", "1", CVAR_ARCHIVE, "Draw the target's name");
  cg_drawWeapon = cgi.AddCvar("cg_drawWeapon", "1", CVAR_ARCHIVE, "Toggle drawing of the weapon model.");
  cg_drawWeaponAlpha = cgi.AddCvar("cg_drawWeaponAlpha", "1", CVAR_ARCHIVE, "The alpha transparency for drawing the weapon model.");
  cg_drawWeaponBob = cgi.AddCvar("cg_drawWeaponBob", "1", CVAR_ARCHIVE, "If the weapon model bobs while moving.");
  cg_drawWeaponX = cgi.AddCvar("cg_drawWeaponX", "0", CVAR_ARCHIVE, "The x offset for drawing the weapon model.");
  cg_drawWeaponY = cgi.AddCvar("cg_drawWeaponY", "0", CVAR_ARCHIVE, "The y offset for drawing the weapon model.");
  cg_drawWeaponZ = cgi.AddCvar("cg_drawWeaponZ", "0", CVAR_ARCHIVE, "The z offset for drawing the weapon model.");
  cg_drawVitalsPulse = cgi.AddCvar("cg_drawVitalsPulse", "1", CVAR_ARCHIVE, "Pulse the vitals when low");
  cg_entityBob = cgi.AddCvar("cg_entityBob", "1", CVAR_ARCHIVE, "Controls the bobbing of items");
  cg_entityRotate = cgi.AddCvar("cg_entityRotate", "1", CVAR_ARCHIVE, "Controls the rotation of items");
  cg_forceSkin = cgi.AddCvar("cg_forceSkin", "", CVAR_ARCHIVE | CVAR_R_MEDIA, "Force all other players to use this model/skin (e.g. \"enforcer/default\").");
  cg_fov = cgi.AddCvar("cg_fov", "110", CVAR_ARCHIVE, "Horizontal field of view, in degrees, at a 16:9 reference aspect ratio. Automatically scaled for your display's actual aspect ratio.");
  cg_fovZoom = cgi.AddCvar("cg_fovZoom", "55", CVAR_ARCHIVE, "Zoomed in field of view");
  cg_fovInterpolate = cgi.AddCvar("cg_fovInterpolate", "1", CVAR_ARCHIVE, "Interpolate between field of view changes (default 1.0).");
  cg_hitSound = cgi.AddCvar("cg_hitSound", "1", CVAR_ARCHIVE, "If a hit sound is played when damaging an enemy.");
  cg_predict = cgi.AddCvar("cg_predict", "1", 0, "Use client side movement prediction");
  cg_quickJoinMaxPing = cgi.AddCvar("cg_quickJoinMaxPing", "200", CVAR_ARCHIVE, "Maximum ping allowed for quick join");
  cg_quickJoinMinClients = cgi.AddCvar("cg_quickJoinMinClients", "1", CVAR_ARCHIVE, "Minimum clients allowed for quick join");
  cg_spritePhysics = cgi.AddCvar("cg_spritePhysics", "1", CVAR_ARCHIVE, "Whether to enable sprite physics or not.");
  // deliberately not archived: the server overrules this whenever it decides what, if anything,
  // is being watched, so a persisted value would be one the player never chose
  cg_cameraMode = cgi.AddCvar("cg_cameraMode", "0", 0,
                               "How the spectator and demo playback camera frames its subject: "
                               "0 first person, 1 third person, 2 follow.");

  cg_thirdPerson = cgi.AddCvar("cg_thirdPerson", "0", CVAR_ARCHIVE | CVAR_DEVELOPER, "Activate third person perspective.");
  cg_thirdPersonX = cgi.AddCvar("cg_thirdPersonX", "-200", CVAR_ARCHIVE, "The x offset for third person perspective.");
  cg_thirdPersonY = cgi.AddCvar("cg_thirdPersonY", "0", CVAR_ARCHIVE, "The y offset for third person perspective.");
  cg_thirdPersonZ = cgi.AddCvar("cg_thirdPersonZ", "40", CVAR_ARCHIVE, "The z offset for third person perspective.");
  cg_thirdPersonPitch = cgi.AddCvar("cg_thirdPersonPitch", "0", CVAR_ARCHIVE, "The pitch offset for third person perspective.");
  cg_thirdPersonYaw = cgi.AddCvar("cg_thirdPersonYaw", "0", CVAR_ARCHIVE, "The yaw offset for third person perspective.");

  cg_autoSwitch = cgi.AddCvar("autoSwitch", "1", CVAR_USER_INFO | CVAR_ARCHIVE, "The weapon auto-switch mode. 0 disables, 1 switches from Blaster only, 2 always switches, 3 switches to new weapons.");
  cg_color = cgi.AddCvar("color", "default", CVAR_USER_INFO | CVAR_ARCHIVE, "Specifies the effect color for your own weapon trails.");
  cg_hand = cgi.AddCvar("hand", "1", CVAR_ARCHIVE | CVAR_USER_INFO, "Controls weapon handedness (center: 0, right: 1, left: 2).");
  cg_helmet = cgi.AddCvar("helmet", "default", CVAR_USER_INFO | CVAR_ARCHIVE, "Specifies your helmet color, in the hex format \"rrggbb\". \"default\" uses the skin or team's defaults.");
#if defined(G_HOOK)
  cg_hookStyle = cgi.AddCvar("hookStyle", "pull", CVAR_USER_INFO | CVAR_ARCHIVE, "Your preferred hook style. Can be either \"pull\", \"swing_manual\", or \"swing_auto\".");
#endif
  cg_pants = cgi.AddCvar("pants", "default", CVAR_USER_INFO | CVAR_ARCHIVE, "Specifies your pants color, in the hex format \"rrggbb\". \"default\" uses the skin or team's defaults.");
  cg_shirt = cgi.AddCvar("shirt", "default", CVAR_USER_INFO | CVAR_ARCHIVE, "Specifies your shirt color, in the hex format \"rrggbb\". \"default\" uses the skin or team's defaults.");
  cg_skin = cgi.AddCvar("skin", "enforcer/default", CVAR_USER_INFO | CVAR_ARCHIVE, "Your player model and skin.");
  cg_skin->Autocomplete = Cg_SkinAutocomplete_f;

  editor = cgi.GetCvar("editor");

  // add forward to server commands for tab completion

  cgi.AddCmd("wave", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("kill", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("use", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("drop", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("say", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("sayTeam", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("info", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("give", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("god", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("noClip", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("weaponLast", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("team", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("teamName", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("teamSkin", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("spectate", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("join", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("ready", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("unready", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("playerList", NULL, CMD_CGAME, NULL);
  cgi.AddCmd("chaseStop", NULL, CMD_CGAME, "Stop chasing and return to free spectator flight.");

  cgi.AddCmd("camera", Cg_CameraModeCycle_f, CMD_CGAME,
             "Cycle the first-person, third-person and follow cameras "
             "(demo playback and spectating).");

  Cg_InitUi();

  Cg_InitHud();

  Cg_InitDiscord();

#if defined(G_TECH)
  Cg_Tech_Init();
#endif
#if defined(G_CTF)
  Cg_Ctf_Init();
#endif

  Cg_Vote_Init();
  Cg_Intermission_Init();

  Cg_Module_Init();

  cge.ClipEntity = Cg_ClipEntity;

  cgi.Print("Client game module initialized\n");

  Cg_InitHudUi();
}

/**
 * @brief Called when switching game directories or quitting.
 */
static void Cg_Shutdown(void) {

  cgi.Print("Client game module shutdown...\n");

  Cg_FreeMedia();

  Cg_ShutdownUi();

  Cg_ShutdownDiscord();

  Cg_Module_Shutdown();

  cgi.FreeTag(MEM_TAG_CGAME_LEVEL);
  cgi.FreeTag(MEM_TAG_CGAME);
}

/**
 * @brief Resolve team info from team configstring
 */
static void Cg_ParseTeamInfo(const char *s) {

  PointerArray *info = $(alloc(PointerArray), initWithDestroy, free);

  char buf[MAX_STRING_CHARS];
  q_strlcpy(buf, s, sizeof(buf));
  char *save = NULL;
  for (char *tok = q_strtok_r(buf, "\\", &save); tok; tok = q_strtok_r(NULL, "\\", &save)) {
    $(info, add, q_strdup(tok));
  }

  const size_t count = info->count;

  if (count != lengthof(cgState.teams) * 4) {
    release(info);
    Cg_Error("Invalid team data: %s\n", s);
  }

  CGameTeamInfo *team = cgState.teams;
  for (size_t i = 0; i < count; i += 4, team++) {

    team->id = atoi((char *) $(info, get, i + 0));

    q_strlcpy(team->name, (char *) $(info, get, i + 1), sizeof(team->name));

    team->hue = atoi((char *) $(info, get, i + 2));

    if (!Color_Parse((char *) $(info, get, i + 3), &team->color)) {
      team->color = color_white;
    }
  }

  release(info);
}

/**
 * @brief The tail of the `Cg_ParseConfigString` chain: nothing claimed, so common parses it.
 */
static bool Cg_ParseConfigString_Common(int32_t index) {
  return false;
}

ParseConfigString Cg_ParseConfigString = Cg_ParseConfigString_Common;

/**
 * @brief An updated configuration string has just been received from the server.
 * Refresh related variables and media that aren't managed by the engine.
 */
static void Cg_UpdateConfigString(int32_t i) {

  if (Cg_ParseConfigString(i)) {
    return;
  }

  const char *s = cgi.ConfigString(i);

  switch (i) {
    case CS_GAMEPLAY:
      cgState.gameplay = (GameplayId) strtol(s, NULL, 10);
      return;
    case CS_NUM_TEAMS:
      cgState.numTeams = Clampf(atoi(s), 0, MAX_TEAMS);
      return;
    case CS_TEAM_INFO:
      Cg_ParseTeamInfo(s);
      return;
    case CS_ITEM_SET:
      cgState.items = (GameItems) strtol(s, NULL, 10);
      return;
#if defined(G_HOOK)
    case CS_HOOK_PULL_SPEED: {
      char *end;
      cgState.hookPullSpeed = strtof(s, &end);
      if (end == s || *end || !isfinite(cgState.hookPullSpeed) || cgState.hookPullSpeed <= 0.f) {
        Cg_Warn("Invalid hook pull speed \"%s\"\n", s);
        cgState.hookPullSpeed = PM_SPEED_HOOK_PULL;
      }
      return;
    }
#endif
    case CS_NAV_EDIT:
      cgState.navEdit = (int32_t) strtol(s, NULL, 10);
      return;
  }

  if (i >= CS_CORPSES && i < CS_CORPSES + MAX_CORPSES) {
    Cg_LoadClient(&cgState.corpses[i - CS_CORPSES], s);
    return;
  }

  if (i >= CS_CLIENTS && i < CS_CLIENTS + MAX_CLIENTS) {

    CGameClientInfo *ci = &cgState.clients[i - CS_CLIENTS];
    Cg_LoadClient(ci, s);

    // the server does not count connected clients for us: the entries it sends are the count
    cgState.numClients = 0;
    for (int32_t j = 0; j < MAX_CLIENTS; j++) {
      if (*cgi.ConfigString(CS_CLIENTS + j)) {
        cgState.numClients++;
      }
    }

    // restart the animation of everyone wearing this client info, since the frames it was
    // resolved against may not be the frames of whatever model just replaced it. A corpse is
    // excepted: it is not its owner, and it keeps the animation it died in however they go on
    // to dress. Without that, a client info sent for any reason at all -- and respawning is
    // one -- played a corpse's death over again where it lay.
    const int32_t clientNum = i - CS_CLIENTS;
    for (int32_t j = 0; j < MAX_ENTITIES; j++) {
      ClientEntity *ent = &cgi.client->entities[j];

      if (ent->current.effects & EF_CORPSE) {
        continue;
      }

      if ((ent->current.effects & EF_CLIENT) && ent->current.client == (uint8_t) clientNum) {
        ent->animation1.time = ent->animation2.time = 0;
        ent->animation1.frame = ent->animation2.frame = -1;
      }
    }
  }
}

/**
 * @brief React to a parsed server command.
 */
/**
 * @brief Renders an incoming chat message.
 * @details The sender arrives as a client number rather than as text, so the message can be
 * attributed, coloured and filtered by who said it instead of by what it happens to say.
 */
static void Cg_Chat(int32_t client, uint8_t flags, const char *message) {

  const bool team = flags & CHAT_TEAM;

  const int32_t color = team ? ESC_COLOR_TEAM_CHAT : ESC_COLOR_CHAT;

  cgi.PrintLevel(PRINT_CHAT, "%s^%d: %s\n", cgState.clients[client].name, color, message);

  // the sound is the module's to choose, because only it knows which kind of message this is
  const char *sample = cgi.GetCvarString(team ? "cl_teamChatSound" : "cl_chatSound");

  if (sample && *sample) {
    Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
      .sample = cgi.LoadSample(sample, ASSET_CONTEXT_SOUNDS),
      .flags = S_PLAY_UI
    });
  }
}

/**
 * @brief Accepts an incoming voice frame.
 * @details Presentation only: the frame has already crossed the network, so declining it here
 * saves nothing. Muting is enforced by the server, which refuses to relay in the first place.
 */
static bool Cg_Voice(int32_t client, uint8_t flags) {
  return true;
}

static void Cg_ParsedMessage(int32_t cmd, void *data) {

  switch (cmd) {
    case SV_CMD_CONFIG_STRING:
      Cg_UpdateConfigString((int32_t) (intptr_t) data);
      break;
  }
}

/**
 * @brief The tail of the `Cg_ParseServerCommand` chain: nothing claimed, so common parses it.
 */
static bool Cg_ParseServerCommand_Common(int32_t cmd) {
  return false;
}

ParseServerCommand Cg_ParseServerCommand = Cg_ParseServerCommand_Common;

/**
 * @brief Parse a single server command, returning true on success.
 */
static bool Cg_ParseMessage(int32_t cmd) {

  if (Cg_ParseServerCommand(cmd)) {
    return true;
  }

  switch (cmd) {
    case SV_CMD_SOUND:
      Cg_ParseSound();
      return true;

    case SV_CMD_TEMP_ENTITY:
      Cg_ParseTempEntity();
      return true;

    case SV_CMD_MUZZLE_FLASH:
      Cg_ParseMuzzleFlash();
      return true;

    case SV_CMD_SCORES:
      Cg_ParseScores();
      return true;

    case SV_CMD_SNAP_ANGLES:
      cgState.snapViewAngles = cgi.ReadAngles();
      cgState.snapAngles = true;
      return true;

    case SV_CMD_CENTER_PRINT:
      Cg_ParseCenterPrint();
      return true;

    case SV_CMD_VIEW_KICK:
      Cg_ParseViewKick();
      return true;

    default:
      break;
  }

  return false;
}

#if defined(G_HOOK)
/**
 * @brief Fetch the server's reported hook pull speed.
 */
float Cg_GetHookPullSpeed(void) {

  return cgState.hookPullSpeed;
}
#endif

/**
 * @brief The tail of the `Cg_ListGameplayModes` hook, offering every mode in
 * `gGameplayModes` - the same table `g_gameplay` is parsed against on the
 * game side, so the name and label a module offers can never drift from what
 * the server will actually coerce it to.
 */
static const Gameplay *Cg_ListGameplayModes_Common(size_t *count) {

  *count = lengthof(gGameplayModes);

  return gGameplayModes;
}

ListGameplayModes Cg_ListGameplayModes = Cg_ListGameplayModes_Common;

/**
 * @brief Clear any state that should not persist over multiple server connections.
 */
static void Cg_ClearState(void) {

  memset(&cgState, 0, sizeof(cgState));

  Cg_ClearInput();

  Cg_FreeEntities();

  Cg_FreeEditorEntities();

  Cg_ClearHud();

  Cg_ClearScores();

  Cg_ClearUi();

  Cg_StateDidClear();
}

/**
 * @brief The tail of the `Cg_StateDidClear` chain: a notification, so it does nothing.
 */
static void Cg_StateDidClear_Common(void) {
}

StateDidClear Cg_StateDidClear = Cg_StateDidClear_Common;

/**
 * @brief Prepares the scene so that early rendering operations may begin.
 */
static void Cg_PrepareScene(const ClientFrame *frame) {

  Cg_PrepareView(frame);

  Cg_PrepareStage(frame);
}

/**
 * @brief Populates the scene with entities, sprites, samples, etc.. for the interpolated frame.
 */
static void Cg_PopulateScene(const ClientFrame *frame) {

  Cg_AddPortals(frame);

  Cg_AddEntities(frame);

  Cg_AddEffects();

  Cg_AddFlares();

  Cg_AddSprites();

  Cg_AddLights();

  Cg_SceneDidPopulate(frame);
}

/**
 * @brief The tail of the `Cg_SceneDidPopulate` chain: a notification, so it does nothing.
 */
static void Cg_SceneDidPopulate_Common(const ClientFrame *frame) {
}

SceneDidPopulate Cg_SceneDidPopulate = Cg_SceneDidPopulate_Common;

/**
 * @brief Hands the frame to the HUD, and to what the HUD still does outside its View hierarchy.
 */
static void Cg_UpdateScreen(const ClientFrame *frame) {

  Cg_UpdateHud(frame);

  // The HUD hides itself in nav edit and shows the instructions instead
  if (!cgState.navEdit) {
    Cg_DrawHud(frame);
  }

  Cg_CheckEditor();

  Cg_ScreenDidUpdate(frame);
}

/**
 * @brief The tail of the `Cg_ScreenDidUpdate` chain: a notification, so it does nothing.
 */
static void Cg_ScreenDidUpdate_Common(const ClientFrame *frame) {
}

ScreenDidUpdate Cg_ScreenDidUpdate = Cg_ScreenDidUpdate_Common;

/**
 * @brief Entry point that populates and returns the cgame export table with all function pointers.
 */
CGameExport *Cg_LoadCgame(CGameImport *import) {

  cgi = *import;

  cge.apiVersion = CGAME_API_VERSION;
  cge.protocol = PROTOCOL_MINOR;
  cge.name = GAME_NAME;

  cge.Init = Cg_Init;
  cge.Shutdown = Cg_Shutdown;
  cge.ClearState = Cg_ClearState;

  cge.LoadMedia = Cg_LoadMedia;
  cge.FreeMedia = Cg_FreeMedia;

  cge.ParsedMessage = Cg_ParsedMessage;
  cge.ParseMessage = Cg_ParseMessage;
  cge.ParseEditorEntity = Cg_ParseEditorEntity;
  cge.Chat = Cg_Chat;
  cge.Voice = Cg_Voice;

  cge.HandleEvent = Cg_HandleEvent;
  cge.Look = Cg_Look;
  cge.Move = Cg_ExportMove;

  cge.Interpolate = Cg_Interpolate;
  cge.UsePrediction = Cg_ExportUsePrediction;
  cge.PredictMovement = Cg_PredictMovement;

  cge.UpdateLoading = Cg_UpdateLoading;
  cge.PrepareScene = Cg_PrepareScene;
  cge.PopulateScene = Cg_PopulateScene;
  cge.PopulateEditorScene = Cg_PopulateEditorScene;
  cge.UpdateScreen = Cg_UpdateScreen;
  cge.UpdateInstaller = Cg_UpdateInstaller;
  cge.UpdateDiscord = Cg_UpdateDiscord;

  return &cge;
}
