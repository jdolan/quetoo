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

#include "g_local.h"

/*
 * G_misc_teleporter_touch
 */
static void G_misc_teleporter_touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {
	g_edict_t *dest;
	vec3_t forward, delta_angles;

	if (!other->client)
		return;

	dest = G_Find(NULL, FOFS(target_name), self->target);

	if (!dest) {
		gi.Debug("G_teleporter_touch: Couldn't find destination.\n");
		return;
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	gi.UnlinkEntity(other);

	VectorCopy(dest->s.origin, other->s.origin);
	VectorCopy(dest->s.origin, other->s.old_origin);
	other->s.origin[2] += 10.0;

	// overwrite velocity and hold them in place briefly
	other->client->ps.pm_state.pm_flags &= ~PMF_TIME_MASK;
	other->client->ps.pm_state.pm_flags = PMF_TIME_TELEPORT;

	other->client->ps.pm_state.pm_time = 20;

	// draw the teleport splash at source and on the player
	self->s.event = EV_CLIENT_TELEPORT;
	other->s.event = EV_CLIENT_TELEPORT;

	// set delta angles
	VectorSubtract(dest->s.angles, other->client->cmd_angles, delta_angles);
	PackAngles(delta_angles, other->client->ps.pm_state.delta_angles);

	AngleVectors(dest->s.angles, forward, NULL, NULL);
	VectorScale(forward, other->client->speed, other->velocity);
	other->velocity[2] = 150.0;

	VectorClear(other->client->cmd_angles);
	VectorClear(other->client->angles);
	VectorClear(other->s.angles);

	G_KillBox(other); // telefrag anyone in our spot

	gi.LinkEntity(other);
}

/*QUAKED misc_teleporter (1 0 0) (-32 -32 -24) (32 32 -16)
 Stepping onto this disc will teleport players to the targeted misc_teleporter_dest object.
 */
void G_misc_teleporter(g_edict_t *ent) {
	vec3_t v;

	if (!ent->target) {
		gi.Debug("G_misc_teleporter: No target specified.\n");
		G_FreeEdict(ent);
		return;
	}

	ent->solid = SOLID_TRIGGER;
	ent->move_type = MOVE_TYPE_NONE;

	if (ent->model) { // model form, trigger_teleporter
		gi.SetModel(ent, ent->model);
		ent->sv_flags = SVF_NO_CLIENT;
	} else { // or model-less form, misc_teleporter
		VectorSet(ent->mins, -32.0, -32.0, -24.0);
		VectorSet(ent->maxs, 32.0, 32.0, -16.0);

		VectorCopy(ent->s.origin, v);
		v[2] -= 16.0;

		// add effect if ent is not burried and effect is not inhibited
		if (!gi.PointContents(v) && !(ent->spawn_flags & 4)) {
			ent->s.effects = EF_TELEPORTER;
			ent->s.sound = gi.SoundIndex("world/teleport_hum");
		}
	}

	ent->touch = G_misc_teleporter_touch;

	gi.LinkEntity(ent);
}

/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16)
 Point teleporters at these.
 */
void G_misc_teleporter_dest(g_edict_t *ent) {
	G_ProjectSpawn(ent);
}

