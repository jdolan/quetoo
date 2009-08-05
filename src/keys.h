/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

#ifndef __KEYS_H__
#define __KEYS_H__

#define KEY_HISTORYSIZE 64
#define KEY_LINESIZE 256

enum QKEYS {
	K_FIRST,

	K_CTRL_A = 1,
	K_CTRL_E = 5,

	K_BACKSPACE = 8,
	K_TAB = 9,
	K_ENTER = 13,
	K_PAUSE = 19,
	K_ESCAPE = 27,
	K_SPACE = 32,
	K_DEL = 127,

	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,
	K_MWHEELDOWN,
	K_MWHEELUP,
	K_MOUSE4,
	K_MOUSE5,

	K_AUX1,
	K_AUX2,
	K_AUX3,
	K_AUX4,
	K_AUX5,
	K_AUX6,
	K_AUX7,
	K_AUX8,
	K_AUX9,
	K_AUX10,
	K_AUX11,
	K_AUX12,
	K_AUX13,
	K_AUX14,
	K_AUX15,
	K_AUX16,

	K_NUMLOCK,

	K_KP_INS,
	K_KP_END,
	K_KP_DOWNARROW,
	K_KP_PGDN,
	K_KP_LEFTARROW,
	K_KP_5,
	K_KP_RIGHTARROW,
	K_KP_HOME,
	K_KP_UPARROW,
	K_KP_PGUP,
	K_KP_DEL,
	K_KP_SLASH,
	K_KP_MINUS,
	K_KP_PLUS,
	K_KP_ENTER,

	K_UPARROW,
	K_DOWNARROW,
	K_RIGHTARROW,
	K_LEFTARROW,

	K_HOME,
	K_END,
	K_PGUP,
	K_PGDN,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_INS,
	K_SHIFT,
	K_CTRL,
	K_ALT,

	K_LAST = 511  // to support as many chars as posible
};

extern int key_numdown;

extern char chat_buffer[];
extern int chat_bufferlen;
extern qboolean chat_team;

void Cl_KeyEvent(unsigned int ascii, unsigned short unicode, qboolean down, unsigned time);
char *Cl_EditLine(void);
void Cl_WriteBindings(FILE *f);
void Cl_InitKeys(void);
void Cl_ShutdownKeys(void);
void Cl_ClearTyping(void);
void Cl_ClearNotify(void);

#endif /* __KEYS_H__ */
