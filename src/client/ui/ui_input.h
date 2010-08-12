/**
 * @file m_input.h
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

#ifndef CLIENT_MENU_M_INPUT_H
#define CLIENT_MENU_M_INPUT_H

/* prototype */
struct menuNode_s;

/* mouse input */
void MN_MouseWheel(qboolean down, int x, int y);
void MN_MouseMove(int x, int y);
void MN_MouseDown(int x, int y, int button);
void MN_MouseUp(int x, int y, int button);
void MN_InvalidateMouse(void);
qboolean MN_CheckMouseMove(void);
struct menuNode_s *MN_GetHoveredNode(void);

/* focus */
void MN_RequestFocus(struct menuNode_s* node);
qboolean MN_HasFocus(const struct menuNode_s* node);
void MN_RemoveFocus(void);
qboolean MN_KeyPressed(unsigned int key, unsigned short unicode);

/* mouse capture */
struct menuNode_s* MN_GetMouseCapture(void);
void MN_SetMouseCapture(struct menuNode_s* node);
void MN_MouseRelease(void);

/* all inputs */
void MN_ReleaseInput(void);

#endif
