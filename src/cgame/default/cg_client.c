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

#include "cg_local.h"

#define DEFAULT_CLIENT_INFO "newbie\\qforcer/enforcer"

/**
 * Cg_LoadClientSkin
 *
 * Parses a single line of a .skin definition file. Note that, unlike Quake3,
 * our skin paths start with players/, not models/players/.
 */
static void Cg_LoadClientSkin(r_image_t **skins, const r_md3_t *md3, char *line) {
	int i;

	if (strstr(line, "tag_"))
		return;

	char *image_name, *mesh_name = line;

	if ((image_name = strchr(mesh_name, ','))) {
		*image_name++ = '\0';

		while (isspace(*image_name)) {
			image_name++;
		}
	} else {
		return;
	}

	const r_md3_mesh_t *mesh = md3->meshes;
	for (i = 0; i < md3->num_meshes; i++, mesh++) {

		if (!strcasecmp(mesh_name, mesh->name)) {
			skins[i] = cgi.LoadImage(image_name, it_diffuse);
			break;
		}
	}
}

/**
 * Cg_LoadClientSkins
 *
 * Parses the appropriate .skin file, resolving skins for each mesh within the
 * model. If a skin can not be resolved for any mesh, the entire skins array is
 * invalidated so that the default will be loaded.
 */
static void Cg_LoadClientSkins(const r_model_t *mod, r_image_t **skins, const char *skin) {
	char path[MAX_QPATH], line[MAX_STRING_CHARS];
	char *buffer;
	int i, j, len;

	// load the skin definition file
	snprintf(path, sizeof(path) - 1, "%s_%s.skin", mod->name, skin);

	if ((len = cgi.LoadFile(path, (void *) &buffer)) == -1) {
		cgi.Debug("Cg_LoadClientSkins: %s not found\n", path);

		skins[0] = NULL;
		return;
	}

	const r_md3_t *md3 = (r_md3_t *) mod->extra_data;

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

		if (skins[i]->type == it_null) {
			cgi.Debug("Cg_LoadClientSkins: %s: %s has no skin\n", path, mesh->name);

			skins[0] = NULL;
			break;
		}
	}
}

/**
 * Cg_ValidateClient
 *
 * Ensures that models and skins were resolved for the specified client info.
 */
static bool Cg_ValidateClient(cl_client_info_t *ci) {

	if (!ci->head || !ci->upper || !ci->lower)
		return false;

	if (!ci->head_skins[0] || !ci->upper_skins[0] || !ci->lower_skins[0])
		return false;

	return true;
}

/**
 * Cg_LoadClient
 *
 * Resolves the player name, model and skins for the specified user info string.
 * If validation fails, we fall back on the DEFAULT_CLIENT_INFO constant.
 */
void Cg_LoadClient(cl_client_info_t *ci, const char *s) {
	char path[MAX_QPATH];
	const char *t;
	char *u, *v;
	int i;

	cgi.Debug("Cg_LoadClient: %s\n", s);

	// copy the entire string
	strncpy(ci->info, s, sizeof(ci->info));
	ci->info[sizeof(ci->info) - 1] = '\0';

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
	strncpy(ci->name, s, sizeof(ci->name));
	ci->name[sizeof(ci->name) - 1] = '\0';

	if ((v = strchr(ci->name, '\\'))) { // check for name\model/skin

		if ((u = strchr(v, '/'))) { // it's well-formed
			*v = *u = '\0';

			strcpy(ci->model, v + 1);
			strcpy(ci->skin, u + 1);
		}
	}

	if (v && u) { // load the models
		snprintf(path, sizeof(path), "players/%s/head.md3", ci->model);
		if ((ci->head = cgi.LoadModel(path))) {
			Cg_LoadClientSkins(ci->head, ci->head_skins, ci->skin);
		}

		snprintf(path, sizeof(path), "players/%s/upper.md3", ci->model);
		if ((ci->upper = cgi.LoadModel(path))) {
			Cg_LoadClientSkins(ci->upper, ci->upper_skins, ci->skin);
		}

		snprintf(path, sizeof(path), "players/%s/lower.md3", ci->model);
		if ((ci->lower = cgi.LoadModel(path))) {
			Cg_LoadClientSkins(ci->lower, ci->lower_skins, ci->skin);
		}
	}

	// ensure we were able to load everything
	if (!Cg_ValidateClient(ci)) {

		if (!strcmp(s, DEFAULT_CLIENT_INFO)) {
			cgi.Error("Cg_LoadClient: Failed to load default client info\n");
		}

		// and if we weren't, use the default
		Cg_LoadClient(ci, DEFAULT_CLIENT_INFO);
	}
}

/**
 * Cg_LoadClients
 *
 * Load all client info strings from the server.
 */
void Cg_LoadClients(void) {
	int i;

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
 * Cg_NextAnimation
 *
 * Returns the next animation to advance to, defaulting to a no-op.
 */
static entity_animation_t Cg_NextAnimation(const entity_animation_t a) {

	switch (a) {
	case ANIM_BOTH_DEATH1:
	case ANIM_BOTH_DEATH2:
	case ANIM_BOTH_DEATH3:
		return a + 1;

	case ANIM_TORSO_GESTURE:
	case ANIM_TORSO_ATTACK1:
	case ANIM_TORSO_ATTACK2:
	case ANIM_TORSO_DROP:
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
 * Cg_AnimateClientEntity_
 *
 * Resolve the frames and interpolation fractions for the specified animation
 * and entity. If a non-looping animation has completed, proceed to the next
 * animation in the sequence.
 */
static void Cg_AnimateClientEntity_(const r_md3_t *md3, cl_entity_animation_t *a, r_entity_t *e) {

	e->frame = e->old_frame = 0;
	e->lerp = 1.0;
	e->back_lerp = 0.0;

	if (a->animation > md3->num_animations) {
		cgi.Warn("Cg_AnimateClientEntity: Invalid animation: %s: %d\n", e->model->name,
				a->animation);
		return;
	}

	const r_md3_animation_t *anim = &md3->animations[a->animation];

	if (!anim->num_frames || !anim->hz) {
		cgi.Warn("Cg_AnimateClientEntity_: Bad animation sequence: %s: %d\n", e->model->name,
				a->animation);
		return;
	}

	const int frame_time = 1000 / anim->hz;
	const int animation_time = anim->num_frames * frame_time;
	const int elapsed_time = cgi.client->time - a->time;
	int frame = elapsed_time / frame_time;

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
			a->time = cgi.client->time;

			Cg_AnimateClientEntity_(md3, a, e);
			return;
		}

		frame = (frame - anim->num_frames) % anim->looped_frames;
	}

	frame = anim->first_frame + frame;

	if (frame != a->frame) { // shuffle the frames
		a->old_frame = a->frame;
		a->frame = frame;
	}

	a->lerp = (elapsed_time % frame_time) / (float) frame_time;
	a->fraction = elapsed_time / (float) animation_time;

	e->frame = a->frame;
	e->old_frame = a->old_frame;
	e->lerp = a->lerp;
	e->back_lerp = 1.0 - a->lerp;
}

/**
 * Cg_AnimateClientEntity
 *
 * Runs the animation sequences for the specified entity, setting the frame
 * indexes and interpolation fractions for the specified renderer entities.
 */
void Cg_AnimateClientEntity(cl_entity_t *e, r_entity_t *upper, r_entity_t *lower) {
	const r_md3_t *md3 = (r_md3_t *) upper->model->extra_data;

	// do the torso animation
	if (e->current.animation1 != e->prev.animation1 || !e->animation1.time) {
		//cgi.Debug("torso: %d -> %d\n", e->current.animation1, e->prev.animation1);
		e->animation1.animation = e->current.animation1 & ~ANIM_TOGGLE_BIT;
		e->animation1.time = cgi.client->time;
	}

	Cg_AnimateClientEntity_(md3, &e->animation1, upper);

	// and then the legs
	if (e->current.animation2 != e->prev.animation2 || !e->animation2.time) {
		//cgi.Debug("legs: %d -> %d\n", e->current.animation2, e->prev.animation2);
		e->animation2.animation = e->current.animation2 & ~ANIM_TOGGLE_BIT;
		e->animation2.time = cgi.client->time;
	}

	Cg_AnimateClientEntity_(md3, &e->animation2, lower);
}
