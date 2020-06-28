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
 * @brief Resolve HSV from the given hue value
 */
vec3_t Cg_ResolveEffectHSV(const float hue, const float default_hue) {

	if (hue < 0) {
		return Vec3(default_hue, 1.f, 1.f);
	} else if (hue > 360.f) {
		return Vec3(0.f, 0.f, 1.f);
	}

	return Vec3(hue, 1.f, 1.f);
}

/**
 * @brief Resolve a client hue from the entity index given in the effect
 */
vec3_t Cg_ResolveEntityEffectHSV(const uint8_t index, const float default_hue) {

	if (index >= MAX_CLIENTS) {
		return Vec3(default_hue, 1.f, 1.f);
	}

	return Cg_ResolveEffectHSV(cgi.client->client_info[index].hue, default_hue);
}

/**
 * @brief
 */
static void Cg_InactiveEffect(cl_entity_t *ent, const vec3_t org) {

	if (Cg_IsSelf(ent) && !cgi.client->third_person) {
		return;
	}

	cgi.AddSprite(&(const r_sprite_t) {
		.origin = Vec3_Add(org, Vec3(0.f, 0.f, 50.f)),
		.color = Color_Color32(color_white),
		.media = (r_media_t *) cg_sprite_inactive,
		.size = 32.f
	});
}

/**
 * @brief Processes the entity's effects mask, augmenting the renderer entity.
 */
void Cg_EntityEffects(cl_entity_t *ent, r_entity_t *e) {

	e->effects = ent->current.effects;

	if (e->effects & EF_ROTATE) {
		const float rotate = cgi.client->unclamped_time;
		e->angles.y = cg_entity_rotate->value * rotate / M_PI;
	}

	if (e->effects & EF_BOB) {
		e->termination = e->origin;
		const float bob = sinf(cgi.client->unclamped_time * 0.005f + ent->current.number);
		e->origin.z += cg_entity_bob->value * bob;
	}

	if (e->effects & EF_PULSE) {
		const float pulse = (cosf(cgi.client->unclamped_time * 0.0033f + ent->current.number) + 1.f);
		const float c = 1.f - (cg_entity_pulse->value * 0.5f * pulse);
		e->color = Vec4(c, c, c, 1.f);
		e->color = Vec4(1.f, 1.f, 1.f, 1.f);
	} else {
		e->color = Vec4(1.f, 1.f, 1.f, 1.f);
	}

	if (e->effects & EF_INACTIVE) {
		Cg_InactiveEffect(ent, e->origin);
	}

	if (e->effects & EF_RESPAWN) {
		const vec3_t color = Vec3(0.5f, 0.5f, 0.f);
		e->shell = Vec3_Add(e->shell, Vec3_Scale(color, 0.5));
	}

	if (e->effects & EF_QUAD) {
		const cg_light_t l = {
			.origin = e->origin,
			.radius = 80.0,
			.color = Vec3(0.3f, 0.7f, 0.7f)
		};

		Cg_AddLight(&l);

		e->shell = Vec3_Add(e->shell, Vec3_Scale(l.color, 0.5));
	}

	if (e->effects & EF_CTF_CARRY) {
		const uint8_t team = (e->effects & EF_CTF_CARRY_BITS) >> EF_CTF_CARRY_OFFSET;
		const vec3_t effect_color = Cg_ResolveEffectHSV(cg_team_info[team].hue, 0.f);

		const cg_light_t l = {
			.origin = e->origin,
			.radius = 80.f,
			.color = Color_Vec3(ColorHSV(effect_color.x, effect_color.y, effect_color.z))
		};

		Cg_AddLight(&l);

		e->shell = Vec3_Add(e->shell, Vec3_Scale(l.color, 0.5));
	}

	if (Vec3_Length(e->shell) > 0.0) {
		e->shell = Vec3_Normalize(e->shell);
		e->effects |= EF_SHELL;
	}

	if (e->effects & EF_DESPAWN) {

		if (!(ent->prev.effects & EF_DESPAWN)) {
			ent->timestamp = cgi.client->unclamped_time;
		}

		e->effects |= (EF_BLEND | EF_NO_SHADOW);
		e->color.w = 1.f - ((cgi.client->unclamped_time - ent->timestamp) / 3000.f);
	}

	if (e->effects & EF_LIGHT) {

		const color_t color = Color32_Color(ent->current.color);

		const cg_light_t l = {
			.origin = e->origin,
			.radius = ent->current.termination.x,
			.color = Color_Vec3(color)
		};

		Cg_AddLight(&l);
	}

	if (ent->current.trail == TRAIL_ROCKET) {
		e->effects |= EF_NO_SHADOW;
	}
}
