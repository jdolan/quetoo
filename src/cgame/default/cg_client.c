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
#include "game/default/bg_pmove.h"

#define DEFAULT_CLIENT_INFO "newbie\\qforcer/default"

/**
 * @brief Parses a single line of a .skin definition file. Note that, unlike Quake3,
 * our skin paths start with players/, not models/players/.
 */
static void Cg_LoadClientSkin(r_material_t **skins, const r_md3_t *md3, char *line) {
	int32_t i;

	if (strstr(line, "tag_"))
		return;

	char *skin_name, *mesh_name = line;

	if ((skin_name = strchr(mesh_name, ','))) {
		*skin_name++ = '\0';

		while (isspace(*skin_name)) {
			skin_name++;
		}
	} else {
		return;
	}

	const r_md3_mesh_t *mesh = md3->meshes;
	for (i = 0; i < md3->num_meshes; i++, mesh++) {

		if (!g_ascii_strcasecmp(mesh_name, mesh->name)) {
			skins[i] = cgi.LoadMaterial(skin_name);
			break;
		}
	}
}

/**
 * @brief Parses the appropriate .skin file, resolving skins for each mesh
 * within the model. If a skin can not be resolved for any mesh, the entire
 * skins array is invalidated so that the default will be loaded.
 */
static void Cg_LoadClientSkins(const r_model_t *mod, r_material_t **skins, const char *skin) {
	char path[MAX_QPATH], line[MAX_STRING_CHARS];
	char *buffer;
	int32_t i, j, len;

	// load the skin definition file
	g_snprintf(path, sizeof(path), "%s_%s.skin", mod->media.name, skin);

	if ((len = cgi.LoadFile(path, (void *) &buffer)) == -1) {
		cgi.Debug("%s not found\n", path);

		skins[0] = NULL;
		return;
	}

	const r_md3_t *md3 = (r_md3_t *) mod->mesh->data;

	i = j = 0;
	memset(line, 0, sizeof(line));

	// parse the skin definition file line-by-line
	while (i < len) {
		const char c = buffer[i++];
		line[j++] = c;

		if (c == '\n' || c == '\r' || i == len) {

			Cg_LoadClientSkin(skins, md3, line);

			j = 0;
			memset(line, 0, sizeof(line));
		}
	}

	// ensure that a skin was resolved for each mesh, nullifying if not

	const r_md3_mesh_t *mesh = md3->meshes;
	for (i = 0; i < md3->num_meshes; i++, mesh++) {

		if (!skins[i]) {
			cgi.Debug("%s: %s has no skin\n", path, mesh->name);

			skins[0] = NULL;
			break;
		}
	}

	cgi.FreeFile(buffer);
}

/**
 * @brief Ensures that models and skins were resolved for the specified client info.
 */
static _Bool Cg_ValidateClient(cl_client_info_t *ci) {

	if (!ci->head || !ci->torso || !ci->legs)
		return false;

	if (!ci->head_skins[0] || !ci->torso_skins[0] || !ci->legs_skins[0])
		return false;

	VectorScale(PM_MINS, PM_SCALE, ci->legs->mins);
	VectorScale(PM_MAXS, PM_SCALE, ci->legs->maxs);

	ci->legs->radius = (ci->legs->maxs[2] - ci->legs->mins[2]) / 2.0;

	return true;
}

/**
 * @brief Resolves the player name, model and skins for the specified user info string.
 * If validation fails, we fall back on the DEFAULT_CLIENT_INFO constant.
 */
void Cg_LoadClient(cl_client_info_t *ci, const char *s) {
	char path[MAX_QPATH];
	const char *t;
	char *u = NULL, *v;
	int32_t i;

	cgi.Debug("%s\n", s);

	// copy the entire string
	g_strlcpy(ci->info, s, sizeof(ci->info));

	i = 0;
	t = s;
	while (*t) { // check for non-printable chars
		if (*t <= 32) {
			i = -1;
			break;
		}
		t++;
	}

	if (*ci->info == '\0' || i == -1) { // use default
		Cg_LoadClient(ci, DEFAULT_CLIENT_INFO);
		return;
	}

	// isolate the player's name
	g_strlcpy(ci->name, s, sizeof(ci->name));

	if ((v = strchr(ci->name, '\\'))) { // check for name\model/skin

		if ((u = strchr(v, '/'))) { // it's well-formed
			*v = *u = '\0';

			strcpy(ci->model, v + 1);
			strcpy(ci->skin, u + 1);
		}
	}

	if (v && u) { // load the models
		g_snprintf(path, sizeof(path), "players/%s/head.md3", ci->model);
		if ((ci->head = cgi.LoadModel(path))) {
			Cg_LoadClientSkins(ci->head, ci->head_skins, ci->skin);
		}

		g_snprintf(path, sizeof(path), "players/%s/upper.md3", ci->model);
		if ((ci->torso = cgi.LoadModel(path))) {
			Cg_LoadClientSkins(ci->torso, ci->torso_skins, ci->skin);
		}

		g_snprintf(path, sizeof(path), "players/%s/lower.md3", ci->model);
		if ((ci->legs = cgi.LoadModel(path))) {
			Cg_LoadClientSkins(ci->legs, ci->legs_skins, ci->skin);
		}

		g_snprintf(path, sizeof(path), "players/%s/%s_i", ci->model, ci->skin);
		ci->icon = cgi.LoadImage(path, IT_PIC);
	}

	// ensure we were able to load everything
	if (!Cg_ValidateClient(ci)) {

		if (!g_strcmp0(s, DEFAULT_CLIENT_INFO)) {
			cgi.Error("Failed to load default client info\n");
		}

		// and if we weren't, use the default
		Cg_LoadClient(ci, DEFAULT_CLIENT_INFO);
	}
}

/**
 * @brief Load all client info strings from the server.
 */
void Cg_LoadClients(void) {
	int32_t i;

	memset(cgi.client->client_info, 0, sizeof(cgi.client->client_info));

	for (i = 0; i < MAX_CLIENTS; i++) {
		cl_client_info_t *ci = &cgi.client->client_info[i];
		const char *s = cgi.ConfigString(CS_CLIENTS + i);

		if (!*s)
			continue;

		Cg_LoadClient(ci, s);
	}
}

/**
 * @brief Returns the next animation to advance to, defaulting to a no-op.
 */
static entity_animation_t Cg_NextAnimation(const entity_animation_t a) {

	switch (a) {
		case ANIM_BOTH_DEATH1:
		case ANIM_BOTH_DEATH2:
		case ANIM_BOTH_DEATH3:
			return a + 1;

		case ANIM_TORSO_DROP:
			return ANIM_TORSO_RAISE;

		case ANIM_TORSO_GESTURE:
		case ANIM_TORSO_ATTACK1:
		case ANIM_TORSO_RAISE:
			return ANIM_TORSO_STAND1;

		case ANIM_LEGS_LAND1:
		case ANIM_LEGS_LAND2:
			return ANIM_LEGS_IDLE;

		default:
			return a;
	}
}

/**
 * @brief Resolve the frames and interpolation fractions for the specified animation
 * and entity. If a non-looping animation has completed, proceed to the next
 * animation in the sequence.
 */
static void Cg_AnimateClientEntity_(const r_md3_t *md3, cl_entity_animation_t *a, r_entity_t *e) {
	static const cvar_t *time_scale;

	if (time_scale == NULL) {
		time_scale = cgi.Cvar("time_scale", NULL, 0, NULL);
	}

	e->frame = e->old_frame = 0;
	e->lerp = 1.0;
	e->back_lerp = 0.0;

	if (a->animation > md3->num_animations) {
		cgi.Warn("Invalid animation: %s: %d\n", e->model->media.name, a->animation);
		return;
	}

	const r_md3_animation_t *anim = &md3->animations[a->animation];

	if (!anim->num_frames || !anim->hz) {
		cgi.Warn("Bad animation sequence: %s: %d\n", e->model->media.name, a->animation);
		return;
	}

	const uint32_t frame_time = (1000 / time_scale->value) / anim->hz;
	const uint32_t animation_time = anim->num_frames * frame_time;
	const uint32_t elapsed_time = cgi.client->systime - a->time;
	int32_t frame = elapsed_time / frame_time;

	if (elapsed_time >= animation_time) { // to loop, or not to loop

		if (!anim->looped_frames) {
			const entity_animation_t next = Cg_NextAnimation(a->animation);

			if (next == a->animation) { // no change, just stay put
				e->frame = anim->first_frame + anim->num_frames - 1;
				e->lerp = 1.0;
				e->back_lerp = 0.0;
				return;
			}

			a->animation = next; // or move into the next animation
			a->time = cgi.client->systime;

			Cg_AnimateClientEntity_(md3, a, e);
			return;
		}

		frame = (frame - anim->num_frames) % anim->looped_frames;
	}

	frame = anim->first_frame + frame;

	if (frame != a->frame) {
		if (a->frame == -1) {
			a->old_frame = frame;
			a->frame = frame;
		} else {
			a->old_frame = a->frame;
			a->frame = frame;
		}
	}

	a->lerp = (elapsed_time % frame_time) / (vec_t) frame_time;
	a->fraction = elapsed_time / (vec_t) animation_time;

	e->frame = a->frame;
	e->old_frame = a->old_frame;
	e->lerp = a->lerp;
	e->back_lerp = 1.0 - a->lerp;
}

/**
 * @brief Runs the animation sequences for the specified entity, setting the frame
 * indexes and interpolation fractions for the specified renderer entities.
 */
void Cg_AnimateClientEntity(cl_entity_t *e, r_entity_t *torso, r_entity_t *legs) {
	const r_md3_t *md3 = (r_md3_t *) torso->model->mesh->data;

	// do the torso animation
	if (e->current.animation1 != e->prev.animation1 || !e->animation1.time) {
		//cgi.Debug("Torso: %d -> %d\n", e->current.animation1, e->prev.animation1);
		e->animation1.animation = e->current.animation1 & ~ANIM_TOGGLE_BIT;
		e->animation1.time = cgi.client->systime;
	}

	Cg_AnimateClientEntity_(md3, &e->animation1, torso);

	// and then the legs
	if (e->current.animation2 != e->prev.animation2 || !e->animation2.time) {
		//cgi.Debug("Legs: %d -> %d\n", e->current.animation2, e->prev.animation2);
		e->animation2.animation = e->current.animation2 & ~ANIM_TOGGLE_BIT;
		e->animation2.time = cgi.client->systime;
	}

	Cg_AnimateClientEntity_(md3, &e->animation2, legs);
}
