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

#include "DemoControlsView.h"

#define _Class _DemoControlsView

/**
 * @brief The playback rates the speed slider offers. The slider's own value is an index into
 * this, not a rate: the rates are not evenly spaced, and a linear slider over 0.25 to 8 would
 * bury everything below 1x - slow motion - in the leftmost tenth of its travel.
 */
static const double demo_speeds[] = { 0.25, 0.5, 0.75, 1.0, 2.0, 4.0, 8.0 };

/**
 * @brief The index in `demo_speeds` of 1x, the rate a demo starts playing at.
 */
#define DEMO_SPEED_DEFAULT 3

/**
 * @brief Resolves the slider index whose rate is closest to `speed`, so the handle can be placed
 * from a time_scale set by anything else (the fast_forward / slow_motion binds, or the console).
 */
static double demoSpeedIndex(double speed) {

  size_t best = DEMO_SPEED_DEFAULT;
  for (size_t i = 0; i < lengthof(demo_speeds); i++) {
    if (fabs(demo_speeds[i] - speed) < fabs(demo_speeds[best] - speed)) {
      best = i;
    }
  }

  return (double) best;
}


#pragma mark - Delegates

/**
 * @brief An image Button for the transport bar. Sizing, padding and pointer events are all
 * styled by `DemoControlsView Button` in ui/common.css; nothing here assigns a styled attribute.
 */
static Button *demoButton(const char *image, ButtonDelegate delegate) {

  Button *button = $(alloc(Button), initWithImage, Cg_LoadImage(image));
  assert(button);

  button->delegate = delegate;

  return button;
}

/**
 * @brief ButtonDelegate for the rewind button.
 */
static void didClickRewind(Button *button) {
  cgi.Cbuf("demo_seek_relative -10000\n");
}

/**
 * @brief ButtonDelegate for the play (resume) button.
 */
static void didClickPlay(Button *button) {
  cgi.Cbuf("demo_pause\n");
}

/**
 * @brief ButtonDelegate for the fast-forward button.
 */
static void didClickFastForward(Button *button) {
  cgi.Cbuf("demo_seek_relative 10000\n");
}

/**
 * @brief SliderDelegate for the scrubber: seeks to an absolute position.
 */
static void didSetScrubber(Slider *slider, double value) {
  cgi.Cbuf(va("demo_seek %d\n", (int32_t) value));
}

/**
 * @brief SliderDelegate for the speed slider: maps the slider's index to a playback rate.
 */
static void didSetSpeed(Slider *slider, double value) {

  const size_t index = (size_t) Clampf((float) value, 0, lengthof(demo_speeds) - 1);

  cgi.ForceSetCvarValue("time_scale", (float) demo_speeds[index]);
}

#pragma mark - DemoControlsView

/**
 * @fn DemoControlsView *DemoControlsView::initWithFrame(DemoControlsView *self, const SDL_Rect *frame)
 * @memberof DemoControlsView
 */
static DemoControlsView *initWithFrame(DemoControlsView *self, const SDL_Rect *frame) {

  self = (DemoControlsView *) super(StackView, self, initWithFrame, frame);
  if (self) {

    StackView *this = (StackView *) self;

    self->rewindButton = demoButton("pics/rewind", (ButtonDelegate) {
      .self = self,
      .didClick = didClickRewind
    });

    $((View *) this, addSubview, (View *) self->rewindButton);

    self->playButton = demoButton("pics/play", (ButtonDelegate) {
      .self = self,
      .didClick = didClickPlay
    });

    $((View *) this, addSubview, (View *) self->playButton);

    self->scrubber = $(alloc(Slider), initWithFrame, NULL);
    assert(self->scrubber);

    $((View *) self->scrubber, addClassName, "scrubber");

    self->scrubber->min = 0.0;
    self->scrubber->delegate.self = self;
    self->scrubber->delegate.didSetValue = didSetScrubber;

    $((View *) this, addSubview, (View *) self->scrubber);

    self->fastForwardButton = demoButton("pics/fast_forward", (ButtonDelegate) {
      .self = self,
      .didClick = didClickFastForward
    });

    $((View *) this, addSubview, (View *) self->fastForwardButton);

    self->speedSlider = $(alloc(Slider), initWithFrame, NULL);
    assert(self->speedSlider);

    $((View *) self->speedSlider, addClassName, "speed");

    self->speedSlider->min = 0.0;
    self->speedSlider->max = lengthof(demo_speeds) - 1;
    self->speedSlider->step = 1.0;
    self->speedSlider->snapToStep = true;
    self->speedSlider->value = demoSpeedIndex(cgi.GetCvarValue("time_scale"));
    self->speedSlider->delegate.self = self;
    self->speedSlider->delegate.didSetValue = didSetSpeed;

    $((View *) this, addSubview, (View *) self->speedSlider);

    // the Slider's own label can only print its value, which here is an index; the rate it maps
    // to gets its own Text, and .speed > .label hides the built-in one
    self->speedLabel = $(alloc(Text), initWithText, NULL, NULL);
    assert(self->speedLabel);

    $((View *) self->speedLabel, addClassName, "speedLabel");

    $((View *) this, addSubview, (View *) self->speedLabel);
    release(self->speedLabel);
  }

  return self;
}

/**
 * @fn bool DemoControlsView::respondToKey(DemoControlsView *self, SDL_Scancode key, bool repeat)
 * @memberof DemoControlsView
 */
static bool respondToKey(DemoControlsView *self, SDL_Scancode key, bool repeat) {

  switch (key) {
    case SDL_SCANCODE_LEFT:
      // seeking repeats, so the key can be held to scan through a recording
      cgi.Cbuf("demo_seek_relative -10000\n");
      return true;
    case SDL_SCANCODE_RIGHT:
      cgi.Cbuf("demo_seek_relative 10000\n");
      return true;

    // pause would flicker and the speed steps would run away at the repeat rate, so these act
    // only on the initial press, while still claiming the key
    case SDL_SCANCODE_SPACE:
      if (!repeat) {
        cgi.Cbuf("demo_pause\n");
      }
      return true;
    case SDL_SCANCODE_COMMA:
      if (!repeat) {
        cgi.Cbuf("slow_motion\n");
      }
      return true;
    case SDL_SCANCODE_PERIOD:
      if (!repeat) {
        cgi.Cbuf("fast_forward\n");
      }
      return true;

    default:
      return false;
  }
}

/**
 * @fn void DemoControlsView::update(DemoControlsView *self, int32_t time, int32_t duration)
 * @memberof DemoControlsView
 */
static void update(DemoControlsView *self, int32_t time, int32_t duration) {

  self->scrubber->max = duration;

  // don't fight the user's own drag with a stale server-reported position
  if (!(self->scrubber->control.state & ControlStateHighlighted)) {
    $((Slider *) self->scrubber, setValue, time);
  }

  const double speed = cgi.GetCvarValue("time_scale");

  if (!(self->speedSlider->control.state & ControlStateHighlighted)) {
    $((Slider *) self->speedSlider, setValue, demoSpeedIndex(speed));
  }

  $(self->speedLabel, setText, va("%gx", speed));
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((DemoControlsViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((DemoControlsViewInterface *) clazz->interface)->respondToKey = respondToKey;
  ((DemoControlsViewInterface *) clazz->interface)->update = update;
}

/**
 * @fn Class *DemoControlsView::_DemoControlsView(void)
 * @memberof DemoControlsView
 */
Class *_DemoControlsView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "DemoControlsView",
      .superclass = _StackView(),
      .instanceSize = sizeof(DemoControlsView),
      .interfaceSize = sizeof(DemoControlsViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
