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
#include "game/common/bg_pmove.h"

InputButton cgButtons[4];

#define CG_FOLLOW_ZOOM_SPEED 400.f
#define CG_FOLLOW_DISTANCE_MIN 40.f
#define CG_FOLLOW_DISTANCE_MAX 800.f

static Cvar *cg_run;

typedef struct {
  Vec3 prev, next, kick;
  uint32_t timestamp;
  uint32_t interval;
} CGameKick;

static CGameKick cgKick;

/**
 * @brief The coloured name of the key bound to the given command, or red `UNBOUND`.
 * @remarks Asking rather than naming the shipped default, which a player may well have moved -
 * on macOS a right click arrives as mouse 3, so `+hook` does not sit where the defaults put it.
 */
const char *Cg_KeyBind(const char *bind) {

  const SDL_Scancode key = cgi.KeyForBind(SDL_SCANCODE_UNKNOWN, bind);

  if (key == SDL_SCANCODE_UNKNOWN) {
    return "^1UNBOUND^7";
  }

  return va("^2%s^7", cgi.KeyName(key));
}

/**
 * @brief Accumulates raw mouse motion into the follow camera's yaw and pitch.
 * @remarks The client applies mouse motion to `cgi.client->angles`, but `Cg_UpdateAngles`
 * overwrites that with the view angles whenever `pm_state.type` is `PM_FREEZE` - which is
 * exactly the state the game module puts a chasing spectator in. The raw event is therefore
 * the only place the viewer's own mouse input survives, so follow reads it here rather than
 * diffing angles that are reset out from under it every frame.
 */
static void Cg_UpdateFollowLook(const SDL_Event *event) {

  if (cgi.GetKeyDest() != KEY_GAME) {
    return;
  }

  if (!Cg_FollowEligible(&cgi.client->frame.ps)) {
    return;
  }

  const float sensitivity = cgi.GetCvarValue("mSensitivity");
  const float invert = cgi.GetCvarValue("mInvert") ? -1.f : 1.f;

  cgState.follow.yaw -= cgi.GetCvarValue("mYaw") * event->motion.xrel * sensitivity;

  cgState.follow.pitch = Clampf(
    cgState.follow.pitch + invert * cgi.GetCvarValue("mPitch") * event->motion.yrel * sensitivity,
    -89.f, 89.f
  );
}

/**
 * @brief Handles SDL events, recreating the framebuffer on window resize or expose, driving the
 *   follow camera from mouse motion, and forwarding the mouse wheel to the editor's entity
 *   selection.
 */
void Cg_HandleEvent(const SDL_Event *event) {

  if (Cg_Intermission_HandleEvent(event)) {
    return;
  }

  switch (event->type) {
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      Cg_CreateFramebuffer();
      break;

    case SDL_EVENT_MOUSE_MOTION:
      Cg_UpdateFollowLook(event);
      break;

    case SDL_EVENT_MOUSE_WHEEL:
      if (event->wheel.y != 0.f) {
        Cg_CycleEditorSelection(event->wheel.y > 0.f ? 1 : -1);
      }
      break;

    default:
      break;
  }
}

/**
 * @brief Parse a view kick message from the server, updating the interpolation target.
 */
void Cg_ParseViewKick(void) {

  const Vec3 kick = MakeVec3(cgi.ReadAngle(), 0.0, cgi.ReadAngle());

  cgKick.prev = cgKick.kick;
  cgKick.next = Vec3_Add(cgKick.prev, kick);

  cgKick.timestamp = cgi.client->unclampedTime;
  cgKick.interval = 64;
}

/**
 * @brief Applies damage kick for the current command, ensuring that kick affects the player's aim.
 */
static void Cg_ViewKick(const PlayerMoveCmd *cmd) {

  if (cgKick.timestamp > cgi.client->unclampedTime) {
    memset(&cgKick, 0, sizeof(cgKick));
  }

  const PlayerState *ps1 = &cgi.client->frame.ps;

  if (cgState.snapAngles) {
    // Snap is handled authoritatively in Cg_UpdateAngles; just clear kick state here.
    memset(&cgKick, 0, sizeof(cgKick));
  } else if (cgi.client->previousFrame) {
      const PlayerState *ps0 = &cgi.client->previousFrame->ps;
      Vec3 delta0 = ps0->pmState.deltaAngles;
      Vec3 delta1 = ps1->pmState.deltaAngles;

      if (!Vec3_Equal(delta0, delta1)) {
        static int32_t frame;

        if (cgi.client->frame.frameNum != frame) {
          Cg_Debug("Delta kick %s\n", vtos(cgKick.kick));
          memset(&cgKick, 0, sizeof(cgKick));

          frame = cgi.client->frame.frameNum;
        }
      }
  }

  const uint32_t delta = cgi.client->unclampedTime - cgKick.timestamp;
  if (delta < cgKick.interval) {
    const float frac = Minf(delta, cmd->msec) / (float) cgKick.interval;

    Vec3 kick;
    kick = Vec3_Subtract(cgKick.next, cgKick.prev);
    kick = Vec3_Scale(kick, frac);

    cgKick.kick = Vec3_Add(cgKick.kick, kick);
    cgi.client->angles = Vec3_Add(cgi.client->angles, kick);

  } else if (!Vec3_Equal(cgKick.kick, Vec3_Zero())) {

    if (cgi.client->frame.ps.pmState.type == PM_DEAD) {
      return;
    }

    const float len = Vec3_Length(cgKick.kick);
    if (len < 0.1) {
      cgi.client->angles = Vec3_Subtract(cgi.client->angles, cgKick.kick);
      memset(&cgKick, 0, sizeof(cgKick));
    } else {

      cgKick.prev = cgKick.kick;
      cgKick.next = Vec3_Zero();

      cgKick.timestamp = cgi.client->unclampedTime;
      cgKick.interval = 240;
    }
  }
}

/**
 * @brief Applies weapon fire recoil animation to the view model.
 */
static void Cg_WeaponKick(const PlayerMoveCmd *cmd) {
  static float kick;

  if (cgi.client->thirdPerson) {
    return;
  }

  const ClientEntity *ent = Cg_Self();

  if (!ent) {
    return;
  }

  float delta = 0.0;

  if (ent->animation1.animation == ANIM_TORSO_ATTACK1 && ent->animation1.fraction <= 0.33) {

    const PlayerState *ps = &cgi.client->frame.ps;

    float degrees, interval = 64.0;

    switch (ps->stats[STAT_WEAPON] & 0xFF) {
      case WEAPON_BLASTER:
        degrees = 1.0;
        break;
      case WEAPON_SHOTGUN:
        degrees = 1.5;
        break;
      case WEAPON_SUPER_SHOTGUN:
        degrees = 2.0;
        break;
      case WEAPON_MACHINEGUN:
        degrees = 6.0;
        interval = 1372.0;
        break;
      case WEAPON_HAND_GRENADE:
        degrees = 2.0;
        break;
      case WEAPON_GRENADE_LAUNCHER:
        degrees = 2.6;
        break;
      case WEAPON_ROCKET_LAUNCHER:
        degrees = 2.4;
        break;
      case WEAPON_HYPERBLASTER:
        degrees = 5.0;
        interval = 1176.0;
        break;
      case WEAPON_LIGHTNING:
        degrees = 2.0;
        interval = 784.0;
        break;
      case WEAPON_RAILGUN:
        degrees = 5.0;
        break;
      case WEAPON_BFG10K:
        degrees = 20.0;
        break;
      default:
        return;
    }

    delta = Minf(degrees - kick, degrees * (cmd->msec / interval));

  } else {
    delta = -Minf(kick, kick * (cmd->msec / 196.0));
  }

  kick += delta;
  cgi.client->angles.x -= delta;
}

/**
 * @brief Augments the view offset and angles for the specified command.
 * @see Cl_Look(PlayerMoveCmd)
 */
void Cg_Look(PlayerMoveCmd *cmd) {

  if (cgi.client->demoServer && cgState.spectate.detached) {
    return; // a camera that has left the recorded player behind does not take their recoil
  }

  Cg_ViewKick(cmd);

  Cg_WeaponKick(cmd);
}

/**
 * @brief Accumulate movement and button interactions for the specified command.
 */
static void Cg_Move_Common(PlayerMoveCmd *cmd) {

  if (cgi.client->demoServer) {

    // attack leaves the recorded player behind, and picks them back up. Live, the game module
    // already does exactly this with the attack button, so only playback needs it here
    if (in_attack.state & BUTTON_STATE_DOWN) {
      cgState.spectate.detached = !cgState.spectate.detached;
      cgState.spectate.initialized = false;
    }

    in_attack.state &= ~BUTTON_STATE_DOWN;
  } else if (in_attack.state & (BUTTON_STATE_HELD | BUTTON_STATE_DOWN)) {
    if (!((in_attack.state & BUTTON_STATE_DOWN) && Cg_AttemptSelectWeapon(&cgi.client->frame.ps))) {
      cmd->buttons |= BUTTON_ATTACK;

      // Encode the pixel-accurate muzzle position as a player-relative offset
      // so the server can use it instead of its hardcoded approximation.
      const CGameClientInfo *ci = &cgState.clients[cgi.client->frame.ps.client];
      if (!Vec3_Equal(ci->weaponMuzzle, Vec3_Zero())) {
        cmd->muzzle = Vec3_Subtract(ci->weaponMuzzle, cgi.client->entity->current.origin);
      }
    }
  }

  // The hook is dead weight while watching someone else - there is no body to swing on - so it
  // cycles how they are framed instead. Attack keeps doing what it always has, leaving a player
  // behind and picking one back up, which is the press you least want happening by reflex
  if ((in_hook.state & BUTTON_STATE_DOWN) && Cg_CameraSubject(&cgi.client->frame.ps)) {
    cgi.Cbuf("camera\n");
    in_hook.state &= ~BUTTON_STATE_DOWN;
  }

  if (in_hook.state & (BUTTON_STATE_HELD | BUTTON_STATE_DOWN)) {
    cmd->buttons |= BUTTON_HOOK;
  }

  if (in_score.state & (BUTTON_STATE_HELD | BUTTON_STATE_DOWN)) {
    cmd->buttons |= BUTTON_SCORE;
  }

  in_attack.state &= ~BUTTON_STATE_DOWN;

  if (cg_run->value) {
    if (in_speed.state & BUTTON_STATE_HELD) {
      cmd->buttons |= BUTTON_WALK;
    }
  } else {
    if (!(in_speed.state & BUTTON_STATE_HELD)) {
      cmd->buttons |= BUTTON_WALK;
    }
  }

  if (Cg_FollowEligible(&cgi.client->frame.ps)) {
    // +forward/+back are otherwise idle whenever the follow camera is active - a chasing
    // spectator's movement
    // is never applied, and demo playback sends no commands at all - so they pan the camera in
    // and out instead. cmd->forward arrives as cl_forwardSpeed * msec * key fraction, so it is
    // divided back down to the milliseconds held before being scaled to a per-second rate
    const float forwardSpeed = cgi.GetCvarValue("cl_forwardSpeed");

    if (forwardSpeed > 0.f) {
      const float millis = cmd->forward / forwardSpeed;

      cgState.follow.distance = Clampf(
        cgState.follow.distance - millis * (CG_FOLLOW_ZOOM_SPEED / 1000.f),
        CG_FOLLOW_DISTANCE_MIN, CG_FOLLOW_DISTANCE_MAX
      );
    }
  }

  if (cgi.client->demoServer && cgState.spectate.detached) {
    Cg_UpdateSpectate(cmd);
  }
}

Move Cg_Move = Cg_Move_Common;

/**
 * @brief The `Move` export. The client holds this rather than the chain head, so
 * that the chain a module installs from `Cg_Module_Init` is the one that gets
 * called.
 */
void Cg_ExportMove(PlayerMoveCmd *cmd) {
  Cg_Move(cmd);
}

/**
 * @brief Clear button states.
 */
void Cg_ClearInput(void) {
  memset(&cgKick, 0, sizeof(cgKick));
  memset(cgButtons, 0, sizeof(cgButtons));
}

static void Cg_Speed_down_f(void) {
  cgi.KeyDown(&in_speed);
}

static void Cg_Speed_up_f(void) {
  cgi.KeyUp(&in_speed);
}

static void Cg_Attack_down_f(void) {
  cgi.KeyDown(&in_attack);
}

static void Cg_Attack_up_f(void) {
  cgi.KeyUp(&in_attack);
}

static void Cg_Hook_down_f(void) {
  cgi.KeyDown(&in_hook);
}

static void Cg_Hook_up_f(void) {
  cgi.KeyUp(&in_hook);
}

static void Cg_Score_down_f(void) {
  cgi.KeyDown(&in_score);
}

static void Cg_Score_up_f(void) {
  cgi.KeyUp(&in_score);
}

/**
 * @brief Begins a push to talk voice transmission.
 * @details Takes an optional channel name, so that a module's own channels can be bound. Without
 * one, holding shift promotes it to the team channel, the way shift sends a chat line as sayTeam:
 * key binds carry no modifier of their own, so one bind has to serve both.
 */
static void Cg_Voice_down_f(void) {

  const char *name = cgi.Argv(1);

  // button commands are passed the scancode and time, so a bare bind presents a number here
  if (name[0] && !isdigit(name[0])) {

    if (!q_strcmp(name, "team")) {
      cgi.StartVoice(VOICE_CHANNEL_TEAM);
    } else if (!q_strcmp(name, "all")) {
      cgi.StartVoice(VOICE_CHANNEL_ALL);
    } else {
      cgi.Print("Unknown voice channel \"%s\"\n", name);
    }

    return;
  }

  const bool team = SDL_GetModState() & SDL_KMOD_SHIFT;

  cgi.StartVoice(team ? VOICE_CHANNEL_TEAM : VOICE_CHANNEL_ALL);
}

static void Cg_Voice_up_f(void) {
  cgi.StopVoice();
}

static void Cg_VoiceTeam_down_f(void) {
  cgi.StartVoice(VOICE_CHANNEL_TEAM);
}

/**
 * @brief Init cgame input system.
 */
void Cg_InitInput(void) {

  cg_run = cgi.AddCvar("cg_run", "1", CVAR_ARCHIVE, NULL);

  cgi.AddCmd("+speed", Cg_Speed_down_f, CMD_CGAME, NULL);
  cgi.AddCmd("-speed", Cg_Speed_up_f, CMD_CGAME, NULL);
  cgi.AddCmd("+attack", Cg_Attack_down_f, CMD_CGAME, NULL);
  cgi.AddCmd("-attack", Cg_Attack_up_f, CMD_CGAME, NULL);
  cgi.AddCmd("+hook", Cg_Hook_down_f, CMD_CGAME, NULL);
  cgi.AddCmd("-hook", Cg_Hook_up_f, CMD_CGAME, NULL);
  cgi.AddCmd("+score", Cg_Score_down_f, CMD_CGAME, NULL);
  cgi.AddCmd("-score", Cg_Score_up_f, CMD_CGAME, NULL);
  cgi.AddCmd("+voice", Cg_Voice_down_f, CMD_CGAME, "Transmit voice chat while held; hold shift for your team.");
  cgi.AddCmd("-voice", Cg_Voice_up_f, CMD_CGAME, NULL);
  cgi.AddCmd("+voiceTeam", Cg_VoiceTeam_down_f, CMD_CGAME, "Transmit voice chat to your team while held.");
  cgi.AddCmd("-voiceTeam", Cg_Voice_up_f, CMD_CGAME, NULL);

  Cg_ClearInput();
}
