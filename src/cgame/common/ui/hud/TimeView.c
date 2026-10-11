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

#include "TimeView.h"

#include <stdlib.h>

#define _Class _TimeView

static char mapName[MAX_QPATH];
static uint32_t mapStartTime;
static bool mapTimerStarted;

static bool TimeView_ParseTime(const char *text, uint32_t *seconds) {

  if (text[0] == '^' && text[1]) {
    text += 2;
  }

  char *end;
  const unsigned long minutes = strtoul(text, &end, 10);
  if (end == text || *end++ != ':') {
    return false;
  }

  const char *secondsText = end;
  const unsigned long remainder = strtoul(secondsText, &end, 10);
  if (end == secondsText || *end || remainder >= 60 || minutes > (UINT32_MAX - remainder) / 60) {
    return false;
  }

  *seconds = (uint32_t) (minutes * 60 + remainder);
  return true;
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((CounterView *) self, initWithCaption, NULL, COUNTER_VIEW_NO_STAT);
}

#pragma mark - CounterView

/**
 * @see CounterView::textForFrame(CounterView *, const ClientFrame *)
 */
static const char *textForFrame(CounterView *self, const ClientFrame *frame) {

  const char *time = cgi.ConfigString(CS_TIME);

  if (frame->ps.stats[STAT_TIME]) {
    const char *currentMap = cgi.ConfigString(CS_BSP);
    if (!mapTimerStarted || Str_Compare(mapName, currentMap)) {
      Str_Copy(mapName, currentMap, sizeof(mapName));
      mapStartTime = frame->time;
      mapTimerStarted = true;
    }

    if (cg_drawTimerCountup->integer) {
      uint32_t remaining;
      uint32_t elapsed;
      const Cvar *timeLimit = cgi.GetCvar("g_timeLimit");

      if (timeLimit && timeLimit->value > 0.f &&
          timeLimit->value <= UINT32_MAX / 60.f &&
          TimeView_ParseTime(time, &remaining)) {
        const uint32_t limit = (uint32_t) (timeLimit->value * 60.f);
        elapsed = limit > remaining ? limit - remaining : 0;
      } else {
        elapsed = frame->time >= mapStartTime ? (frame->time - mapStartTime) / 1000 : 0;
      }

      snprintf(self->text, sizeof(self->text), "%2u:%02u", elapsed / 60, elapsed % 60);
      return self->text;
    }
  }

  if (time[0] == '^' && time[1] == '7') {
    return time + 2;
  }

  return time;
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {
  ((ViewInterface *) clazz->interface)->init = init;
  ((CounterViewInterface *) clazz->interface)->textForFrame = textForFrame;
}

/**
 * @fn Class *TimeView::_TimeView(void)
 * @memberof TimeView
 */
Class *_TimeView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "TimeView",
      .superclass = _CounterView(),
      .instanceSize = sizeof(TimeView),
      .interfaceSize = sizeof(TimeViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
