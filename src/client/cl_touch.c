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

#include "cl_local.h"
#include "cl_touch.h"

/*
 * Virtual on-screen touch controls. SCAFFOLD ONLY.
 *
 * Full design, mappings, and the (deferred) one-line hookups into cl_input.c
 * and the cgame are documented in android/CONTROLS.md. Tracking: #856 (item 6).
 *
 * The bodies below are intentionally stubs marked `// TODO(#856)`; this file is
 * compiled and linked but is not yet wired into the event pump or usercmd path,
 * so it cannot affect desktop behavior. See CONTROLS.md §"Integration points".
 */

cvar_t *cg_touch_controls;

/**
 * @brief Singleton virtual-controls state. Populated by Cl_TouchEvent(), drained
 * by Cl_ApplyTouchToMove()/Cl_ApplyTouchToLook() each client frame.
 */
static cl_touch_state_t cl_touch;

/**
 * @brief Default normalized layout constants (fractions of the window). These
 * mirror the values documented in CONTROLS.md §"Default layout".
 */
#define TOUCH_STICK_CENTER_X   0.18f
#define TOUCH_STICK_CENTER_Y   0.74f
#define TOUCH_STICK_RADIUS     0.14f

#define TOUCH_FIRE_CENTER_X    0.84f
#define TOUCH_FIRE_CENTER_Y    0.74f
#define TOUCH_JUMP_CENTER_X    0.90f
#define TOUCH_JUMP_CENTER_Y    0.52f
#define TOUCH_BUTTON_RADIUS    0.08f

#define TOUCH_MENU_CENTER_X    0.95f
#define TOUCH_MENU_CENTER_Y    0.06f
#define TOUCH_MENU_RADIUS      0.05f

/**
 * @brief True if the touch overlay should be active.
 * @remarks Gate for every caller. Desktop returns false because the cvar
 * defaults to 0 there; on Android it defaults to 1 (see Cl_TouchInit()).
 */
bool Cl_TouchControlsActive(void) {

  // TODO(#856): return cg_touch_controls && cg_touch_controls->integer &&
  //   cls.state == CL_ACTIVE && cls.key_state.dest == KEY_GAME &&
  //   SDL_GetNumTouchDevices()-equivalent (SDL3: enumerate via
  //   SDL_GetTouchDevices()). For the scaffold we report inactive so nothing
  //   reads uninitialized layout. See CONTROLS.md §"Gating".
  (void) cl_touch;
  return false;
}

/**
 * @brief Helper: define a round widget region centered at (cx,cy) with `radius`,
 * all in normalized [0..1] screen space.
 */
static cl_touch_region_t Cl_TouchRound(cl_touch_widget_t widget, float cx, float cy, float radius) {

  return (cl_touch_region_t) {
    .widget = widget,
    .mins = Vec2(cx - radius, cy - radius),
    .maxs = Vec2(cx + radius, cy + radius),
    .radius = radius,
  };
}

/**
 * @brief Builds the default widget layout into cl_touch.regions.
 * @remarks Rebuild this when orientation / safe-area changes (Android lifecycle).
 * Coordinates are normalized, so no pixel math here; conversion happens at draw.
 */
static void Cl_UpdateTouchLayout(void) {

  cl_touch.regions[TOUCH_WIDGET_MOVE_STICK] =
    Cl_TouchRound(TOUCH_WIDGET_MOVE_STICK, TOUCH_STICK_CENTER_X, TOUCH_STICK_CENTER_Y, TOUCH_STICK_RADIUS);

  // The look widget has no fixed rect: any FINGER_DOWN on the right half of the
  // screen that did not hit a button starts a look drag. Encoded as the right
  // half so hit-testing is uniform; see Cl_TouchHitTest() TODO.
  cl_touch.regions[TOUCH_WIDGET_LOOK] = (cl_touch_region_t) {
    .widget = TOUCH_WIDGET_LOOK,
    .mins = Vec2(0.5f, 0.0f),
    .maxs = Vec2(1.0f, 1.0f),
    .radius = 0.0f,
  };

  cl_touch.regions[TOUCH_WIDGET_FIRE] =
    Cl_TouchRound(TOUCH_WIDGET_FIRE, TOUCH_FIRE_CENTER_X, TOUCH_FIRE_CENTER_Y, TOUCH_BUTTON_RADIUS);
  cl_touch.regions[TOUCH_WIDGET_JUMP] =
    Cl_TouchRound(TOUCH_WIDGET_JUMP, TOUCH_JUMP_CENTER_X, TOUCH_JUMP_CENTER_Y, TOUCH_BUTTON_RADIUS);
  cl_touch.regions[TOUCH_WIDGET_MENU] =
    Cl_TouchRound(TOUCH_WIDGET_MENU, TOUCH_MENU_CENTER_X, TOUCH_MENU_CENTER_Y, TOUCH_MENU_RADIUS);
}

/**
 * @brief Registers `cg_touch_controls` and builds the default widget layout.
 * @remarks Hookup (deferred, see CONTROLS.md): call from Cl_InitInput() in
 * cl_input.c, right after the other input cvars are added.
 */
void Cl_TouchInit(void) {

  // On a touch device this should default on; on desktop, off. Detect at runtime
  // rather than compile time so a desktop with a touchscreen can opt in.
  // TODO(#856): const char *def = (SDL_GetTouchDevices count > 0) ? "1" : "0";
  cg_touch_controls = Cvar_Add("cg_touch_controls", "0", CVAR_ARCHIVE,
      "Draw and enable on-screen virtual touch controls.");

  Cl_UpdateTouchLayout();
  Cl_ClearTouch();
}

/**
 * @brief Clears all finger tracking and per-frame button state.
 * @remarks Hookup (deferred): call from Cl_ClearInput() in cl_input.c so touch
 * state resets on level change / disconnect alongside the keyboard buttons.
 */
void Cl_ClearTouch(void) {

  memset(cl_touch.fingers, 0, sizeof(cl_touch.fingers));
  memset(cl_touch.pressed, 0, sizeof(cl_touch.pressed));
  cl_touch.move_axis = Vec2_Zero();
  cl_touch.look_delta = Vec2_Zero();
  cl_touch.menu_pressed_edge = false;
}

/**
 * @brief Hit-tests a normalized point against the widget regions.
 * @return The widget the point falls in, or TOUCH_WIDGET_NONE.
 */
static cl_touch_widget_t Cl_TouchHitTest(const vec2_t point) {

  // TODO(#856): test the discrete buttons (fire/jump/menu) and the move stick
  // first (round: distance(point, center) <= radius), then fall back to the
  // LOOK half-screen rect. Priority matters so a button on the right half is not
  // swallowed by LOOK. See CONTROLS.md §"Hit testing & finger ownership".
  (void) point;
  return TOUCH_WIDGET_NONE;
}

/**
 * @brief Finds the tracking slot for `finger`, or a free slot, or NULL if full.
 */
static cl_touch_finger_t *Cl_TouchFingerSlot(SDL_FingerID finger, bool allocate) {

  for (size_t i = 0; i < TOUCH_MAX_FINGERS; i++) {
    if (cl_touch.fingers[i].active && cl_touch.fingers[i].finger == finger) {
      return &cl_touch.fingers[i];
    }
  }

  if (allocate) {
    for (size_t i = 0; i < TOUCH_MAX_FINGERS; i++) {
      if (!cl_touch.fingers[i].active) {
        return &cl_touch.fingers[i];
      }
    }
  }

  return NULL;
}

/**
 * @brief Feeds one SDL3 finger event into the touch state machine.
 * @see cl_touch.h for contract.
 */
bool Cl_TouchEvent(const SDL_TouchFingerEvent *event) {

  if (!Cl_TouchControlsActive()) {
    return false;
  }

  // SDL3 normalized coordinates: event->x, event->y in [0..1]; event->dx/dy are
  // normalized deltas; event->fingerID identifies the contact across down..up.
  const vec2_t point = Vec2(event->x, event->y);

  switch (event->type) {

    case SDL_EVENT_FINGER_DOWN: {
      // TODO(#856): claim a finger slot, hit-test `point` to bind a widget,
      //   record start/current, and for momentary widgets set pressed[]/edge.
      //   For the move stick, also seed move_axis at (0,0). Return true if a
      //   widget was claimed.
      cl_touch_finger_t *f = Cl_TouchFingerSlot(event->fingerID, true);
      if (f) {
        const cl_touch_widget_t w = Cl_TouchHitTest(point);
        *f = (cl_touch_finger_t) {
          .active = true,
          .finger = event->fingerID,
          .widget = w,
          .start = point,
          .current = point,
        };
        return w != TOUCH_WIDGET_NONE;
      }
      return false;
    }

    case SDL_EVENT_FINGER_MOTION: {
      // TODO(#856): for the bound widget, update current; the move stick updates
      //   move_axis (clamped to radius -> [-1..1]); the LOOK widget accumulates
      //   look_delta += (dx,dy). Buttons ignore motion. See CONTROLS.md.
      cl_touch_finger_t *f = Cl_TouchFingerSlot(event->fingerID, false);
      if (f) {
        f->current = point;
        return f->widget != TOUCH_WIDGET_NONE;
      }
      return false;
    }

    case SDL_EVENT_FINGER_UP: {
      // TODO(#856): release the slot; clear pressed[] for momentary widgets and
      //   zero move_axis if it was the move finger. Leave look_delta to be
      //   drained by Cl_ApplyTouchToLook(). See CONTROLS.md.
      cl_touch_finger_t *f = Cl_TouchFingerSlot(event->fingerID, false);
      if (f) {
        const bool owned = f->widget != TOUCH_WIDGET_NONE;
        memset(f, 0, sizeof(*f));
        return owned;
      }
      return false;
    }

    default:
      return false;
  }
}

/**
 * @brief Applies the virtual move stick to the movement command.
 * @see Cl_Move(pm_cmd_t) in cl_input.c — this is the deferred call site.
 */
void Cl_ApplyTouchToMove(pm_cmd_t *cmd) {

  if (!Cl_TouchControlsActive()) {
    return;
  }

  // TODO(#856): mirror Cl_Move()'s scaling. The keyboard path does:
  //   cmd->forward += cl_forward_speed->value * cmd->msec * Cl_KeyState(&in_forward,...)
  //   cmd->right   += cl_right_speed->value   * cmd->msec * Cl_KeyState(&in_move_right,...)
  // For touch, move_axis.y/.x already give a continuous [-1..1], so:
  //   cmd->forward += (int16_t) (cl_forward_speed->value * cmd->msec * move_axis.y);
  //   cmd->right   += (int16_t) (cl_right_speed->value   * cmd->msec * move_axis.x);
  // (cl_forward_speed/cl_right_speed are file-static in cl_input.c; either expose
  // them or read via Cvar_Get — see CONTROLS.md §"Feeding the usercmd".)
  // The JUMP button maps to the up axis like +move_up:
  //   if (pressed[TOUCH_WIDGET_JUMP]) cmd->up += (int16_t)(cl_up_speed->value * cmd->msec);
  (void) cmd;
}

/**
 * @brief Applies the accumulated look drag to the view angles.
 * @see Cl_Look(pm_cmd_t) in cl_input.c — this is the deferred call site.
 */
void Cl_ApplyTouchToLook(pm_cmd_t *cmd) {

  if (!Cl_TouchControlsActive()) {
    return;
  }

  // TODO(#856): mirror Cl_MouseMotionEvent()/Cl_Look(). Convert the normalized
  // look_delta to degrees using m_yaw/m_pitch * m_sensitivity, respecting
  // m_invert, then:
  //   cl.angles.y -= look_delta.x * m_yaw->value   * m_sensitivity->value * scale;
  //   cl.angles.x += look_delta.y * m_pitch->value * m_sensitivity->value * scale;
  //   cl.angles.x is then clamped by Cl_ClampPitch() which Cl_Look() already calls.
  // Drain the accumulator so deltas are not double-applied:
  cl_touch.look_delta = Vec2_Zero();
  (void) cmd;
}

/**
 * @brief Draws the virtual controls overlay (stick + buttons).
 * @remarks Engine-side scaffold. Preferred shipping path is the cgame HUD via
 * cgi.Draw2DImage / cgi.Draw2DFill from Cg_UpdateScreen(); see CONTROLS.md
 * §"Where the overlay is drawn".
 */
void Cl_DrawTouchControls(void) {

  if (!Cl_TouchControlsActive()) {
    return;
  }

  // TODO(#856): for each region, convert normalized mins/maxs to pixels using
  // r_context.w / r_context.h, then draw a semi-transparent quad/icon
  // (R_Draw2DImage / R_Draw2DFill engine-side, or cgi.Draw2DImage in the cgame).
  // The move stick draws a base ring at center plus a thumb dot offset by
  // move_axis. See CONTROLS.md §"Default layout" for sizes.
}
