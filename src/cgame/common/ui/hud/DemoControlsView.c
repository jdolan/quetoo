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
 * @brief The minimum and maximum values for the speed slider, matching Cl_AdjustDemoPlayback's
 * own clamp on the time_scale cvar.
 */
#define DEMO_SPEED_MIN 1.0
#define DEMO_SPEED_MAX 4.0


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
 * @brief SliderDelegate for the speed slider: adjusts time_scale directly.
 */
static void didSetSpeed(Slider *slider, double value) {
  cgi.ForceSetCvarValue("time_scale", (float) value);
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

    self->speedSlider->min = DEMO_SPEED_MIN;
    self->speedSlider->max = DEMO_SPEED_MAX;
    self->speedSlider->step = 0.5;
    self->speedSlider->value = cgi.GetCvarValue("time_scale");
    self->speedSlider->delegate.self = self;
    self->speedSlider->delegate.didSetValue = didSetSpeed;

    $((Slider *) self->speedSlider, setLabelFormat, "%.1fx");

    $((View *) this, addSubview, (View *) self->speedSlider);
  }

  return self;
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

  if (!(self->speedSlider->control.state & ControlStateHighlighted)) {
    $((Slider *) self->speedSlider, setValue, cgi.GetCvarValue("time_scale"));
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((DemoControlsViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
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
