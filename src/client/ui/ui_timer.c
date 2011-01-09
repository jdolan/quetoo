/**
 * @file m_timer.c
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

#include "ui_nodes.h"
#include "ui_timer.h"

#include "client.h"

/**
 * @brief Number max of timer slot.
 */
#define MN_TIMER_SLOT_NUMBER 10

/**
 * @brief Timer slot. Only one.
 */
static menuTimer_t mn_timerSlots[MN_TIMER_SLOT_NUMBER];

/**
 * @brief First timer from the timer list.
 * This list is ordered from smaller to bigger nextTime value
 */
static menuTimer_t *mn_firstTimer;

/**
 * @brief Remove a timer from the active linked list
 * @note The function doesn't set to null next and previous attributes of the timer
 */
static inline void MN_RemoveTimerFromActiveList (menuTimer_t *timer)
{
	assert(timer >= mn_timerSlots && timer < mn_timerSlots + MN_TIMER_SLOT_NUMBER);
	if (timer->prev) {
		timer->prev->next = timer->next;
	} else {
		mn_firstTimer = timer->next;
	}
	if (timer->next) {
		timer->next->prev = timer->prev;
	}
}

/**
 * @brief Insert a timer in a sorted linked list of timers.
 * List are ordered from smaller to bigger nextTime value
 */
static void MN_InsertTimerInActiveList (menuTimer_t* first, menuTimer_t* newTimer)
{
	menuTimer_t* current = first;
	menuTimer_t* prev = NULL;

	/* find insert position */
	if (current != NULL) {
		prev = current->prev;
	}
	while (current) {
		if (newTimer->nextTime < current->nextTime)
			break;
		prev = current;
		current = current->next;
	}

	/* insert between previous and current */
	newTimer->prev = prev;
	newTimer->next = current;
	if (current != NULL) {
		current->prev = newTimer;
	}
	if (prev != NULL) {
		prev->next = newTimer;
	} else {
		mn_firstTimer = newTimer;
	}
}

/**
 * @brief Internal function to handle timers
 */
void MN_HandleTimers (void)
{
	/* is first element is out of date? */
	while (mn_firstTimer && mn_firstTimer->nextTime <= cls.real_time) {
		menuTimer_t *timer = mn_firstTimer;

		/* throw event */
		timer->calledTime++;
		timer->callback(timer->owner, timer);

		/* update the sorted list */
		if (timer->isRunning) {
			MN_RemoveTimerFromActiveList(timer);
			timer->nextTime += timer->delay;
			MN_InsertTimerInActiveList(timer->next, timer);
		}
	}
}

/**
 * @brief Allocate a new time for a node
 * @param[in] node node parent of the timer
 * @param[in] firstDelay millisecond delay to wait the callback
 * @param[in] callback callback function to call every delay
 */
menuTimer_t* MN_AllocTimer (menuNode_t *node, int firstDelay, timerCallback_t callback)
{
	menuTimer_t *timer = NULL;
	int i;

	/* search empty slot */
	for (i = 0; i < MN_TIMER_SLOT_NUMBER; i++) {
		if (mn_timerSlots[i].callback != NULL)
			continue;
		timer = mn_timerSlots + i;
		break;
	}
	if (timer == NULL)
		Com_Error(ERR_FATAL, "MN_AllocTimer: No more timer slot");

	timer->owner = node;
	timer->delay = firstDelay;
	timer->callback = callback;
	timer->calledTime = 0;
	timer->prev = NULL;
	timer->next = NULL;
	timer->isRunning = qfalse;
	return timer;
}

/**
 * @brief Restart a timer
 */
void MN_TimerStart (menuTimer_t *timer)
{
	if (timer->isRunning)
		return;
	assert(mn_firstTimer != timer && timer->prev == NULL && timer->next == NULL);
	timer->nextTime = cls.real_time + timer->delay;
	timer->isRunning = qtrue;
	MN_InsertTimerInActiveList(mn_firstTimer, timer);
}

/**
 * @brief Stop a timer
 */
void MN_TimerStop (menuTimer_t *timer)
{
	if (!timer->isRunning)
		return;
	MN_RemoveTimerFromActiveList(timer);
	timer->prev = NULL;
	timer->next = NULL;
	timer->isRunning = qfalse;
}

/**
 * @brief Release the timer. It no more exists
 */
void MN_TimerRelease (menuTimer_t *timer)
{
	MN_RemoveTimerFromActiveList(timer);
	timer->prev = NULL;
	timer->next = NULL;
	timer->owner = NULL;
	timer->callback = NULL;
}

#ifdef DEBUG

static menuNode_t *dummyNode = (menuNode_t *) 0x1;

static timerCallback_t dummyCallback = (timerCallback_t) 0x1;

/**
 * @brief unittest to trust a little the linked list
 */
void MN_UnittestTimer (void)
{
	menuTimer_t *a, *b, *c;
	a = MN_AllocTimer(dummyNode, 10, dummyCallback);
	b = MN_AllocTimer(dummyNode, 20, dummyCallback);
	c = MN_AllocTimer(dummyNode, 30, dummyCallback);
	assert(mn_firstTimer == NULL);

	MN_TimerStart(b);
	assert(mn_firstTimer == b);

	MN_TimerStart(a);
	assert(mn_firstTimer == a);

	MN_TimerStart(c);
	assert(mn_firstTimer->next->next == c);

	MN_TimerStop(a);
	MN_TimerStop(b);
	assert(a->owner != NULL);
	assert(mn_firstTimer == c);
	assert(mn_firstTimer->next == NULL);

	MN_TimerStart(a);
	assert(mn_firstTimer == a);
	assert(mn_firstTimer->next == c);

	MN_InsertTimerInActiveList(a->next, b);
	assert(mn_firstTimer == a);
	assert(mn_firstTimer->next == b);
	assert(mn_firstTimer->next->next == c);

	MN_TimerRelease(b);
	assert(mn_firstTimer == a);
	assert(mn_firstTimer->next == c);

	MN_TimerRelease(a);
	assert(mn_firstTimer == c);

	MN_TimerRelease(c);
	assert(mn_firstTimer == NULL);
	assert(c->owner == NULL);
}

#endif
