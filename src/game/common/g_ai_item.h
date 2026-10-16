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

#pragma once

#if defined(__G_LOCAL_H__)

bool G_Ai_CanPickup(const GameClient *cl, const GameEntity *other);

/**
 * @return The share, from 0 to 1, of the item that the client would gain: the health or armor it
 * adds up to its limit, or the ammo that fits. A weapon that the client lacks is worth all of it,
 * and one that it holds only its ammo.
 */
float G_Ai_ItemNeed(const GameClient *cl, const GameEntity *other);
#endif
