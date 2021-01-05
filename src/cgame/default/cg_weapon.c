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

#include "cg_local.h"

/**
 * @brief Adds weapon bob due to running, walking, crouching, etc.
 */
static void Cg_WeaponBob(const player_state_t *ps, vec3_t *offset, vec3_t *angles) {
	const vec3_t bob = Vec3(0.2f, 0.4f, 0.2f);

	*offset = Vec3_Add(*offset, Vec3_Scale(bob, cg_view.bob));

	*angles = Vec3_Add(*angles, Vec3(0.f, 1.5f * cg_view.bob, 0.f));
}

/**
 * @brief Calculates a kick offset and angles based on our player's animation state.
 */
static void Cg_WeaponOffset(cl_entity_t *ent, vec3_t *offset, vec3_t *angles) {

	const vec3_t drop_raise_offset = Vec3(-4.f, -4.f, -4.f);
	const vec3_t drop_raise_angles = Vec3(25.f, -35.f, 2.f);

	const vec3_t kick_offset = Vec3(-6.f, 0.f, 0.f);
	const vec3_t kick_angles = Vec3(-2.f, 0.f, 0.f);

	*offset = Vec3_Zero();
	*angles = Vec3_Zero();

	if (ent->animation1.animation == ANIM_TORSO_DROP) {
		*offset = Vec3_Add(*offset, Vec3_Scale(drop_raise_offset, ent->animation1.fraction));
		*angles = Vec3_Scale(drop_raise_angles, ent->animation1.fraction);
	} else if (ent->animation1.animation == ANIM_TORSO_RAISE) {
		*offset = Vec3_Add(*offset, Vec3_Scale(drop_raise_offset, 1.f - ent->animation1.fraction));
		*angles = Vec3_Scale(drop_raise_angles, 1.f - ent->animation1.fraction);
	} else if (ent->animation1.animation == ANIM_TORSO_ATTACK1) {
		*offset = Vec3_Add(*offset, Vec3_Scale(kick_offset, 1.f - ent->animation1.fraction));
		*angles = Vec3_Scale(kick_angles, 1.f - ent->animation1.fraction);
	}

	*offset = Vec3_Scale(*offset, cg_bob->value);
	*angles = Vec3_Scale(*angles, cg_bob->value);

	*offset = Vec3_Add(*offset, Vec3(cg_draw_weapon_x->value, cg_draw_weapon_y->value, cg_draw_weapon_z->value));
}

/**
 * @brief Periodically calculates the player's velocity, and interpolates it
 * over a small interval to smooth out rapid changes.
 */
static void Cg_SpeedModulus(const player_state_t *ps, vec3_t *offset) {
	static vec3_t old_speed, new_speed;
	static uint32_t time;

	if (cgi.client->unclamped_time < time) {
		time = 0;

		old_speed = Vec3_Zero();
		new_speed = Vec3_Zero();
	}

	vec3_t speed;

	const uint32_t delta = cgi.client->unclamped_time - time;
	if (delta < 100) {
		const float lerp = delta / 100.f;

		speed.x = old_speed.x + lerp * (new_speed.x - old_speed.x);
		speed.y = old_speed.y + lerp * (new_speed.y - old_speed.y);
		speed.z = old_speed.z + lerp * (new_speed.z - old_speed.z);
	} else {
		old_speed = new_speed;

		new_speed.x = -Clampf(ps->pm_state.velocity.x / 200.f, -1.f, 1.f);
		new_speed.y = -Clampf(ps->pm_state.velocity.y / 200.f, -1.f, 1.f);
		new_speed.z = -Clampf(ps->pm_state.velocity.z / 200.f, -.3f, 1.f);

		speed = old_speed;

		time = cgi.client->unclamped_time;
	}

	if (cg_draw_weapon_bob->modified) {
		cg_draw_weapon_bob->value = Clampf(cg_draw_weapon_bob->value, 0.0, 2.0);
		cg_draw_weapon_bob->modified = false;
	}

	*offset = Vec3_Scale(speed, cg_draw_weapon_bob->value);
}

/**
 * @brief Adds the first-person weapon model to the view.
 */
void Cg_AddWeapon(cl_entity_t *ent, r_entity_t *self) {
	static r_entity_t w;
	vec3_t offset, angles;
	vec3_t velocity;

	const player_state_t *ps = &cgi.client->frame.ps;

	if (!cg_draw_weapon->value) {
		return;
	}

	if (cgi.client->third_person) {
		return;
	}

	if (ps->stats[STAT_HEALTH] <= 0) {
		return; // dead
	}

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return; // spectating
	}

	if (!ps->stats[STAT_WEAPON]) {
		return; // no weapon, e.g. level intermission
	}

	memset(&w, 0, sizeof(w));

	w.origin = cgi.view->origin;

	Cg_WeaponOffset(ent, &offset, &angles);
	Cg_WeaponBob(ps, &offset, &angles);
	Cg_SpeedModulus(ps, &velocity);

	w.origin = Vec3_Add(w.origin, velocity);

	switch (cg_hand->integer) {
		case HAND_LEFT:
			offset.y -= 5.f;
			break;
		case HAND_RIGHT:
			offset.y += 5.f;
			break;
		default:
			break;
	}

	w.origin = Vec3_Add(w.origin, Vec3_Scale(cgi.view->up, offset.z));
	w.origin = Vec3_Add(w.origin, Vec3_Scale(cgi.view->right, offset.y));
	w.origin = Vec3_Add(w.origin, Vec3_Scale(cgi.view->forward, offset.x));

	w.angles = Vec3_Add(cgi.view->angles, angles);

	w.effects = EF_WEAPON | EF_NO_SHADOW;

	w.color = Vec4(1.0, 1.0, 1.0, 1.0);

	if (cg_draw_weapon_alpha->value < 1.0) {
		w.effects |= EF_BLEND;
		w.color.w = cg_draw_weapon_alpha->value;
	}

	w.effects |= self->effects & EF_SHELL;
	w.shell = self->shell;

	w.model = cgi.client->models[ps->stats[STAT_WEAPON]];

	w.abs_mins = Vec3_Add(cgi.view->origin, Vec3(-8.f, -8.f, -8.f));
	w.abs_maxs = Vec3_Add(cgi.view->origin, Vec3( 8.f,  8.f,  8.f));

	w.lerp = w.scale = 1.0;

	cgi.AddEntity(cgi.view, &w);
}
