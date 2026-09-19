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

#include "g_types.h"

#if defined(__G_LOCAL_H__)
void G_ClientBegin(GameClient *cl);
void G_ClientBeginFrame(GameClient *cl);
bool G_ClientConnect(GameClient *cl, char *user_info);
void G_ClientDisconnect(GameClient *cl);
void G_ClientRespawn(GameClient *cl, bool voluntary);
void G_ClientThink(GameClient *cl, PlayerMoveCmd *cmd);
Box3 G_ClientStandingBounds(const GameClient *cl);
void G_ClientUserInfoChanged(GameClient *cl, const char *user_info);
void G_Giblets(const GameGiblets *giblets);
#endif
bool G_ClientCanHearVoice(const GameClient *speaker, const GameClient *listener, uint8_t channel);
