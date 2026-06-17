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

#include "cl_types.h"

/*
 * Virtual on-screen touch controls for the Android (and iOS) port.
 *
 * Design and full integration plan: android/CONTROLS.md
 * Tracking issue: jdolan/quetoo#856 (item 6), shared with iOS #855.
 *
 * This unit translates SDL3 finger events into the *same* usercmd inputs the
 * desktop mouse/keyboard path produces, so the network protocol, prediction,
 * and game module are entirely unaffected:
 *
 *   - left  virtual joystick  -> pm_cmd_t.forward / pm_cmd_t.right
 *   - right look drag         -> cl.angles (yaw/pitch), like Cl_MouseMotionEvent
 *   - fire button             -> BUTTON_ATTACK   (set in the cgame, see CONTROLS.md)
 *   - jump button             -> pm_cmd_t.up     (the +move_up axis)
 *   - menu / back button      -> Cl_SetKeyDest() (KEY_UI), like the Esc/AC_BACK path
 *
 * Everything below is gated on `cg_touch_controls` and is a no-op on desktop.
 */

/**
 * @brief The discrete on-screen widgets the overlay exposes.
 * @remarks Order is significant for the `cl_touch_state_t.buttons` array.
 */
typedef enum {
  TOUCH_WIDGET_NONE = -1,

  TOUCH_WIDGET_MOVE_STICK,  // left thumb: forward/side movement
  TOUCH_WIDGET_LOOK,        // right half of the screen: free-look drag (no fixed rect)
  TOUCH_WIDGET_FIRE,        // BUTTON_ATTACK
  TOUCH_WIDGET_JUMP,        // +move_up
  TOUCH_WIDGET_MENU,        // open menu / back (KEY_UI)

  TOUCH_WIDGET_TOTAL
} cl_touch_widget_t;

/**
 * @brief Maximum simultaneously tracked fingers.
 * @remarks SDL3 reports a distinct SDL_FingerID per contact; we map each one to
 * at most one widget. Two is the practical minimum (move + look/fire); a few
 * spare slots allow e.g. move + look + fire + jump concurrently.
 */
#define TOUCH_MAX_FINGERS 8

/**
 * @brief A single tracked finger and the widget it has claimed for its lifetime.
 * @details A finger is bound to a widget on FINGER_DOWN (by hit-testing its
 * start position) and stays bound until FINGER_UP, even if it drags outside the
 * widget's rect. This is what makes a thumbstick feel sticky.
 */
typedef struct {
  bool active;
  SDL_FingerID finger;        // SDL3 finger identity (stable down..up)
  cl_touch_widget_t widget;   // widget claimed at FINGER_DOWN, or TOUCH_WIDGET_NONE
  vec2_t start;               // normalized [0..1] position at FINGER_DOWN
  vec2_t current;             // normalized [0..1] latest position
} cl_touch_finger_t;

/**
 * @brief A rectangular hit region for a widget, in normalized [0..1] screen space.
 * @remarks Normalized so the layout is resolution/orientation independent; it is
 * converted to pixels with `r_context.w/h` only at draw time.
 */
typedef struct {
  cl_touch_widget_t widget;
  vec2_t mins;  // top-left, normalized
  vec2_t maxs;  // bottom-right, normalized
  float radius; // for round widgets (the move stick); 0 for rectangular buttons
} cl_touch_region_t;

/**
 * @brief Aggregate virtual-controls state, produced by Cl_TouchEvent() and
 * consumed by Cl_ApplyTouchToMove() / Cl_ApplyTouchToLook() each frame.
 */
typedef struct {

  /**
   * @brief Live finger tracking table.
   */
  cl_touch_finger_t fingers[TOUCH_MAX_FINGERS];

  /**
   * @brief Static widget layout (normalized). Rebuilt when the cvar/orientation
   * changes; see Cl_TouchInit() / Cl_UpdateTouchLayout().
   */
  cl_touch_region_t regions[TOUCH_WIDGET_TOTAL];

  /**
   * @brief Per-widget pressed flag for this frame, indexed by cl_touch_widget_t.
   * @remarks The move stick and look use the finger table directly; the discrete
   * buttons (fire/jump/menu) read this.
   */
  bool pressed[TOUCH_WIDGET_TOTAL];

  /**
   * @brief Move-stick displacement, normalized to [-1..1] on each axis once the
   * finger has been clamped to the stick radius. (0,0) when no move finger.
   */
  vec2_t move_axis;

  /**
   * @brief Accumulated look delta (normalized screen units) since the last
   * Cl_ApplyTouchToLook(); drained each frame, mirroring relative mouse motion.
   */
  vec2_t look_delta;

  /**
   * @brief Edge flags for momentary actions, latched by Cl_TouchEvent() and
   * cleared by their consumer (e.g. menu open). Avoids retriggering while held.
   */
  bool menu_pressed_edge;

} cl_touch_state_t;

#if defined(__CL_LOCAL_H__)

extern cvar_t *cg_touch_controls;

/**
 * @brief True if the touch overlay should be active (touch-capable device and
 * `cg_touch_controls` enabled and we're in game). Single gate for all callers.
 */
bool Cl_TouchControlsActive(void);

/**
 * @brief Registers `cg_touch_controls` and builds the default widget layout.
 */
void Cl_TouchInit(void);

/**
 * @brief Clears all finger tracking and per-frame button state.
 */
void Cl_ClearTouch(void);

/**
 * @brief Feeds one SDL3 finger event into the touch state machine.
 * @param event The SDL_EVENT_FINGER_DOWN / _UP / _MOTION payload.
 * @return True if the event was consumed by a control widget, false to let it
 * fall through (e.g. a tap on empty HUD space that the UI might want).
 * @remarks Call site (to be added later): Cl_HandleSystemEvent() in cl_input.c.
 */
bool Cl_TouchEvent(const SDL_TouchFingerEvent *event);

/**
 * @brief Applies the virtual move stick to the movement command.
 * @param cmd The usercmd being assembled this frame.
 * @remarks Call site (to be added later): end of Cl_Move() in cl_input.c.
 */
void Cl_ApplyTouchToMove(pm_cmd_t *cmd);

/**
 * @brief Applies the accumulated look drag to the view angles.
 * @param cmd The usercmd being assembled this frame.
 * @remarks Call site (to be added later): end of Cl_Look() in cl_input.c.
 */
void Cl_ApplyTouchToLook(pm_cmd_t *cmd);

/**
 * @brief Draws the virtual controls overlay (stick + buttons).
 * @remarks This is engine-side draw scaffolding. The shipping overlay is more
 * naturally drawn by the cgame via cgi.Draw2DImage/Draw2DFill from
 * Cg_UpdateScreen(); see CONTROLS.md "Where the overlay is drawn". This entry
 * point exists so the unit is self-contained and testable.
 */
void Cl_DrawTouchControls(void);

#endif /* __CL_LOCAL_H__ */
