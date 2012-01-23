/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "cl_local.h"

/*
 * Cl_ParseTempEntity
 */
void Cl_ParseTempEntity(void) {
	vec3_t pos, pos2, dir;
	int i, j;

	const byte type = Msg_ReadByte(&net_message);

	switch (type) {

	case TE_TRACER:
		Msg_ReadPos(&net_message, pos);
		Msg_ReadPos(&net_message, pos2);
		Cl_TracerEffect(pos, pos2);
		break;

	case TE_BULLET: // bullet hitting wall
		Msg_ReadPos(&net_message, pos);
		Msg_ReadDir(&net_message, dir);
		Cl_BulletEffect(pos, dir);
		break;

	case TE_BURN: // burn mark on wall
		Msg_ReadPos(&net_message, pos);
		Msg_ReadDir(&net_message, dir);
		i = Msg_ReadByte(&net_message);
		Cl_BurnEffect(pos, dir, i);
		break;

	case TE_BLOOD: // projectile hitting flesh
		Msg_ReadPos(&net_message, pos);
		Msg_ReadDir(&net_message, dir);
		Cl_BloodEffect(pos, dir, 12);
		break;

	case TE_GIB: // player over-death
		Msg_ReadPos(&net_message, pos);
		Cl_GibEffect(pos, 12);
		break;

	case TE_SPARKS: // colored sparks
		Msg_ReadPos(&net_message, pos);
		Msg_ReadDir(&net_message, dir);
		Cl_SparksEffect(pos, dir, 12);
		break;

	case TE_HYPERBLASTER: // hyperblaster hitting wall
		Msg_ReadPos(&net_message, pos);
		Cl_HyperblasterEffect(pos);
		break;

	case TE_LIGHTNING: // lightning discharge in water
		Msg_ReadPos(&net_message, pos);
		Cl_LightningEffect(pos);
		break;

	case TE_RAIL: // railgun effect
		Msg_ReadPos(&net_message, pos);
		Msg_ReadPos(&net_message, pos2);
		i = Msg_ReadLong(&net_message);
		j = Msg_ReadByte(&net_message);
		Cl_RailEffect(pos, pos2, i, j);
		break;

	case TE_EXPLOSION: // rocket and grenade explosions
		Msg_ReadPos(&net_message, pos);
		Cl_ExplosionEffect(pos);
		break;

	case TE_BFG: // bfg explosion
		Msg_ReadPos(&net_message, pos);
		Cl_BfgEffect(pos);
		break;

	case TE_BUBBLES: // bubbles chasing projectiles in water
		Msg_ReadPos(&net_message, pos);
		Msg_ReadPos(&net_message, pos2);
		Cl_BubbleTrail(pos, pos2, 1.0);
		break;

	default:
		Com_Warn("Cl_ParseTempEntity: Unknown type: %d\n.", type);
		return;
	}
}
