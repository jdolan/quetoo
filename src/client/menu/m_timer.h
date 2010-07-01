/**
 * @file m_timer.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_MENU_M_TIMER_H
#define CLIENT_MENU_M_TIMER_H

struct menuNode_s;
struct menuTimer_s;

typedef void (*timerCallback_t)(struct menuNode_s *node, struct menuTimer_s *timer);

/**
 * @todo We can use void* for the owner type, and allow to use it outside nodes
 */
typedef struct menuTimer_s {
	struct menuTimer_s *next;	/**< next timer in the ordered list of active timers */
	struct menuTimer_s *prev;	/**< previous timer in the ordered list of active timers */
	int nextTime;				/**< next time we must call the callback function. Must node be edited, it used to sort linkedlist of timers */

	struct menuNode_s *owner;	/**< owner node of the timer */
	timerCallback_t callback;	/**< function called every delay */
	int calledTime;				/**< time we call the function. For the first call the value is 1 */

	int delay;					/**< delay in millisecond between each call of */
	void *userData;				/**< free to use data, not used by the core functions */
	qboolean isRunning;			/**< true if the timer is running */
} menuTimer_t;

menuTimer_t* MN_AllocTimer(struct menuNode_s *node, int firstDelay, timerCallback_t callback) __attribute__ ((warn_unused_result));
void MN_TimerStart(menuTimer_t *timer);
void MN_TimerStop(menuTimer_t *timer);
void MN_TimerRelease(menuTimer_t *timer);

void MN_HandleTimers(void);

#ifdef DEBUG
void MN_UnittestTimer(void);
#endif

#endif
