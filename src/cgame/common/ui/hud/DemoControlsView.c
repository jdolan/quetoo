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

#include "DemoControlsView.h"

#define _Class _DemoControlsView

#pragma mark - Delegates

/**
 * @brief ButtonDelegate for the rewind button.
 */
static void didClickRewind(Button *button) {
  cgi.Cbuf("demo_seek_relative -5000\n");
}

/**
 * @brief ButtonDelegate for the step back button.
 */
static void didClickStepBack(Button *button) {
  cgi.Cbuf(va("demo_seek_relative %d\n", -QUETOO_TICK_MILLIS));
}

/**
 * @brief ButtonDelegate for the step forward button.
 */
static void didClickStepForward(Button *button) {
  cgi.Cbuf(va("demo_seek_relative %d\n", QUETOO_TICK_MILLIS));
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
  cgi.Cbuf("demo_seek_relative 5000\n");
}

/**
 * @brief SliderDelegate for the scrubber: records where to seek to.
 * @remarks Recorded rather than sent, because a drag fires this on every mouse motion. See
 * `DemoControlsView::update`, which is what actually sends it.
 */
static void didSetScrubber(Slider *slider, double value) {

  DemoControlsView *this = slider->delegate.self;

  this->pendingSeek = (int32_t) value;
}

#pragma mark - View

/**
 * @see View::respondToEvent(View *, const SDL_Event *)
 * @remarks HudViewController dispatches here directly. Views are not in the responder chain for
 * key events, and this one is hidden during playback in any case, which is when pause has to
 * work. Nothing is forwarded to super for the same reason: bubbling would walk a chain this View
 * was never in, pushing an MVC_VIEW_EVENT for every ancestor on every keystroke.
 */
static void respondToEvent(View *self, const SDL_Event *event) {

  if (event->type != SDL_EVENT_KEY_DOWN) {
    return;
  }

  switch (event->key.scancode) {

    case SDL_SCANCODE_LEFT:
      cgi.Cbuf(va("demo_seek_relative %d\n", -QUETOO_TICK_MILLIS));
      break;
    case SDL_SCANCODE_RIGHT:
      cgi.Cbuf(va("demo_seek_relative %d\n", QUETOO_TICK_MILLIS));
      break;

    case SDL_SCANCODE_SPACE:
      if (!event->key.repeat) {
        cgi.Cbuf("demo_pause\n");
      }
      break;
    case SDL_SCANCODE_COMMA:
      if (!event->key.repeat) {
        cgi.Cbuf("demo_playbackSlower\n");
      }
      break;
    case SDL_SCANCODE_PERIOD:
      if (!event->key.repeat) {
        cgi.Cbuf("demo_playbackFaster\n");
      }
      break;

    default:
      break;
  }
}

#pragma mark - DemoControlsView

/**
 * @fn DemoControlsView *DemoControlsView::initWithFrame(DemoControlsView *self, const SDL_Rect *frame)
 * @memberof DemoControlsView
 */
static DemoControlsView *initWithFrame(DemoControlsView *self, const SDL_Rect *frame) {

  self = (DemoControlsView *) super(StackView, self, initWithFrame, frame);
  if (self) {

    Outlet outlets[] = MakeOutlets(
      MakeOutlet("rewind", &self->rewindButton),
      MakeOutlet("stepBack", &self->stepBackButton),
      MakeOutlet("play", &self->playButton),
      MakeOutlet("scrubber", &self->scrubber),
      MakeOutlet("stepForward", &self->stepForwardButton),
      MakeOutlet("fastForward", &self->fastForwardButton),
      MakeOutlet("speed", &self->speedSlider)
    );

    View *this = (View *) self;

    $(this, awakeWithResourceName, "ui/hud/DemoControlsView.json");
    $(this, resolve, outlets);

    self->rewindButton->delegate.self = self;
    self->rewindButton->delegate.didClick = didClickRewind;

    self->stepBackButton->delegate.self = self;
    self->stepBackButton->delegate.didClick = didClickStepBack;

    self->stepForwardButton->delegate.self = self;
    self->stepForwardButton->delegate.didClick = didClickStepForward;

    self->playButton->delegate.self = self;
    self->playButton->delegate.didClick = didClickPlay;

    self->fastForwardButton->delegate.self = self;
    self->fastForwardButton->delegate.didClick = didClickFastForward;

    self->scrubber->delegate.self = self;
    self->scrubber->delegate.didSetValue = didSetScrubber;

    self->pendingSeek = -1;
  }

  return self;
}

/**
 * @fn void DemoControlsView::update(DemoControlsView *self, int32_t time, int32_t duration)
 * @memberof DemoControlsView
 */
static void update(DemoControlsView *self, int32_t time, int32_t duration) {

  self->scrubber->max = duration;

  // don't fight the user's own drag, nor a seek that has not landed yet, with a stale
  // server-reported position
  if (!(self->scrubber->control.state & ControlStateHighlighted) && self->pendingSeek < 0) {
    $((Slider *) self->scrubber, setValue, time);
  }

  // every seek is a console command the client forwards to the server, which drops a client
  // that issues more than CMD_MAX_STRINGS of them in one server frame. A drag produces one per
  // mouse motion, so send the latest destination at the server's own rate and drop the rest:
  // seeking more finely than the server ticks would buy nothing anyway
  if (self->pendingSeek >= 0) {
    const uint64_t now = SDL_GetTicks();
    if (now - self->lastSeek >= QUETOO_TICK_MILLIS) {
      cgi.Cbuf(va("demo_seek %d\n", self->pendingSeek));
      self->pendingSeek = -1;
      self->lastSeek = now;
    }
  }

  // the slider owns timeScale, so only pull from the cvar when the user isn't dragging
  if (!(self->speedSlider->control.state & ControlStateHighlighted)) {
    $((View *) self->speedSlider, updateBindings, NULL);
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->respondToEvent = respondToEvent;

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
