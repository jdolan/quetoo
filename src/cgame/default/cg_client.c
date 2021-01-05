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

#define DEFAULT_CLIENT_MODEL "qforcer"
#define DEFAULT_CLIENT_SKIN "default"
#define DEFAULT_CLIENT_INFO "newbie\\" DEFAULT_CLIENT_MODEL "/" DEFAULT_CLIENT_SKIN "\\-1\\default\\default\\default"

/**
 * @brief Parses a single line of a .skin definition file. Note that, unlike Quake3,
 * our skin paths start with players/, not models/players/.
 */
static void Cg_LoadClientSkin(r_material_t **skins, const r_mesh_model_t *model, char *line) {
	int32_t i;

	if (strstr(line, "tag_")) {
		return;
	}

	char *skin_name, *face_name = line;

	if ((skin_name = strchr(face_name, ','))) {
		*skin_name++ = '\0';

		while (isspace(*skin_name)) {
			skin_name++;
		}
	} else {
		return;
	}

	const r_mesh_face_t *face = model->faces;
	for (i = 0; i < model->num_faces; i++, face++) {

		if (!g_ascii_strcasecmp(face_name, face->name)) {
			skins[i] = cgi.LoadMaterial(skin_name, ASSET_CONTEXT_PLAYERS);
			break;
		}
	}
}

/**
 * @brief Parses the appropriate .skin file, resolving skins for each face
 * within the model. If a skin can not be resolved for any face, the entire
 * skins array is invalidated so that the default will be loaded.
 */
static _Bool Cg_LoadClientSkins(const r_model_t *mod, r_material_t **skins, const char *skin) {
	char path[MAX_QPATH], line[MAX_STRING_CHARS];
	char *buffer;
	int32_t i, j;
	int64_t len;

	// load the skin definition file
	g_snprintf(path, sizeof(path), "%s_%s.skin", mod->media.name, skin);

	if ((len = cgi.LoadFile(path, (void *) &buffer)) == -1) {
		Cg_Debug("%s not found\n", path);

		skins[0] = NULL;
		return false;
	}

	const r_mesh_model_t *model = mod->mesh;

	i = j = 0;
	memset(line, 0, sizeof(line));

	// parse the skin definition file line-by-line
	while (i < len) {
		const char c = buffer[i++];
		line[j++] = c;

		if (c == '\n' || c == '\r' || i == len) {

			Cg_LoadClientSkin(skins, model, g_strstrip(line));

			j = 0;
			memset(line, 0, sizeof(line));
		}
	}

	// ensure that a skin was resolved for each face, nullifying if not
	const r_mesh_face_t *face = model->faces;
	for (i = 0; i < model->num_faces; i++, face++) {

		if (!skins[i]) {
			Cg_Debug("%s: %s has no skin\n", path, face->name);

			skins[0] = NULL;
			break;
		}
	}

	cgi.FreeFile(buffer);
	return skins[0] != NULL;
}

/**
 * @brief Ensures that models and skins were resolved for the specified client info.
 */
static _Bool Cg_ValidateSkin(cl_client_info_t *ci) {

	if (!ci->head || !ci->torso || !ci->legs) {
		return false;
	}

	if (!ci->head_skins[0] || !ci->torso_skins[0] || !ci->legs_skins[0]) {
		return false;
	}

	return true;
}

/**
 * @brief Resolve and load the specified model/skin for the player.
 */
static _Bool Cg_LoadClientModel(cl_client_info_t *ci, const char *model, const char *skin) {

	g_strlcpy(ci->model, model, sizeof(ci->model));
	g_strlcpy(ci->skin, skin, sizeof(ci->skin));

	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "players/%s/head.md3", ci->model);
	if ((ci->head = cgi.LoadModel(path)) &&
		Cg_LoadClientSkins(ci->head, ci->head_skins, ci->skin)) {

		g_snprintf(path, sizeof(path), "players/%s/upper.md3", ci->model);
		if ((ci->torso = cgi.LoadModel(path)) &&
			Cg_LoadClientSkins(ci->torso, ci->torso_skins, ci->skin)) {

			g_snprintf(path, sizeof(path), "players/%s/lower.md3", ci->model);
			if ((ci->legs = cgi.LoadModel(path)) &&
				Cg_LoadClientSkins(ci->legs, ci->legs_skins, ci->skin)) {

				g_snprintf(path, sizeof(path), "players/%s/%s_i.tga", ci->model, ci->skin);
				ci->icon = cgi.LoadImage(path, IT_PIC);

				if (ci->icon->type == IT_PIC) {
					return true;
				}
			}
		}
	}

	Cg_Debug("Could not load client model %s/%s\n", model, skin);

	// if we reach here, something up above didn't load
	return false;
}

/**
 * @brief Resolves the player name, model and skins for the specified user info string.
 * If validation fails, we fall back on the DEFAULT_CLIENT_INFO constant.
 */
void Cg_LoadClient(cl_client_info_t *ci, const char *s) {
	const char *t;
	char *v = NULL;
	int32_t i;

	Cg_Debug("%s\n", s);

	// copy the entire string
	g_strlcpy(ci->info, s, sizeof(ci->info));

	i = 0;
	t = s;
	while (*t) { // check for non-printable chars
		if (*t < 32 || *t >= 127) {
			i = -1;
			break;
		}
		t++;
	}

	if (*ci->info == '\0' || i == -1) { // use default
		Cg_LoadClient(ci, DEFAULT_CLIENT_INFO);
		return;
	}

	// split info into tokens
	gchar **info = g_strsplit(s, "\\", 0);

	if (g_strv_length(info) != MAX_CLIENT_INFO_ENTRIES) { // invalid info
		Cg_LoadClient(ci, DEFAULT_CLIENT_INFO);
	} else {

		// copy in the name, first token
		g_strlcpy(ci->name, info[0], sizeof(ci->name));

		// check for valid skin
		if ((v = strchr(info[1], '/'))) { // it's well-formed
			*v = '\0';

			// load the models
			if (!Cg_LoadClientModel(ci, info[1], v + 1)) {
				if (!Cg_LoadClientModel(ci, info[1], DEFAULT_CLIENT_SKIN)) {
					if (!Cg_LoadClientModel(ci, DEFAULT_CLIENT_MODEL, DEFAULT_CLIENT_SKIN)) {
						cgi.Error("Failed to load default client skin %s/%s\n",
							DEFAULT_CLIENT_MODEL, DEFAULT_CLIENT_SKIN);
					}
				}
			}
		}

		if (!Color_Parse(info[2], &ci->shirt)) {
			ci->shirt.a = 0;
		}

		if (!Color_Parse(info[3], &ci->pants)) {
			ci->pants.a = 0;
		}

		if (!Color_Parse(info[4], &ci->helmet)) {
			ci->helmet.a = 0;
		}

		const int32_t hue = atoi(info[5]);
		if (hue >= 0) {
			ci->hue = hue;
		} else {
			ci->hue = -1;
		}

		// ensure we were able to load everything
		if (!Cg_ValidateSkin(ci)) {

			if (!g_strcmp0(s, DEFAULT_CLIENT_INFO)) {
				cgi.Error("Failed to load default client info\n");
			}
		}

		ci->legs->mins = Vec3_Scale(PM_MINS, PM_SCALE);
		ci->legs->maxs = Vec3_Scale(PM_MAXS, PM_SCALE);

		ci->legs->radius = (ci->legs->maxs.z - ci->legs->mins.z) / 2.0;

		// load sound files if we're in-game
		if (*cgi.state > CL_DISCONNECTED) {
			cgi.LoadClientModelSamples(ci->model);
		}
	}

	g_strfreev(info);
}

/**
 * @brief Load all client info strings from the server.
 */
void Cg_LoadClients(void) {

	memset(cgi.client->client_info, 0, sizeof(cgi.client->client_info));

	for (int32_t i = 0; i < MAX_CLIENTS; i++) {
		cl_client_info_t *ci = &cgi.client->client_info[i];
		const char *s = cgi.ConfigString(CS_CLIENTS + i);

		if (!*s) {
			continue;
		}

		if (i ^ 1) {
			cgi.LoadingProgress(-1, ci->model);
		}

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
		case ANIM_LEGS_TURN:
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
static void Cg_AnimateClientEntity_(const r_model_t *model, cl_entity_animation_t *a) {
	const r_mesh_model_t *mesh = model->mesh;

	if ((int32_t) a->animation > mesh->num_animations) {
		cgi.Warn("Invalid animation: %s: %d\n", model->media.name, a->animation);
		return;
	}

	const r_mesh_animation_t *anim = &mesh->animations[a->animation];

	if (!anim->num_frames || !anim->hz) {
		cgi.Warn("Bad animation sequence: %s: %d\n", model->media.name, a->animation);
		return;
	}

	const uint32_t frame_duration = 1000 / anim->hz;
	const uint32_t animation_duration = anim->num_frames * frame_duration;
	const uint32_t elapsed_time = cgi.client->unclamped_time - a->time;
	int32_t frame = elapsed_time / frame_duration;

	if (elapsed_time >= animation_duration) { // to loop, or not to loop

		if (!anim->looped_frames) {
			const entity_animation_t next = Cg_NextAnimation(a->animation);

			if (next == a->animation) { // no change, just stay put
				a->old_frame = a->frame;
				a->lerp = a->fraction = 1.0;
				return;
			}

			a->animation = next; // or move into the next animation
			a->time = cgi.client->unclamped_time;

			Cg_AnimateClientEntity_(model, a);
			return;
		}

		frame = (frame - anim->num_frames) % anim->looped_frames;
	}

	if (a->reverse) {
		frame = (anim->num_frames - 1) - frame;
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

	a->lerp = (elapsed_time % frame_duration) / (float) frame_duration;
	a->fraction = Clampf(elapsed_time / (float) animation_duration, 0.0, 1.0);
}

/**
 * @brief Runs the animation sequences for the specified entity, setting the frame
 * indexes and interpolation fractions for the specified renderer entities.
 */
static void Cg_AnimateClientEntity(cl_entity_t *ent, r_entity_t *torso, r_entity_t *legs) {

	const cl_client_info_t *ci = &cgi.client->client_info[ent->current.client];

	Cg_AnimateClientEntity_(ci->torso, &ent->animation1);

	if (torso) {
		torso->frame = ent->animation1.frame;
		torso->old_frame = ent->animation1.old_frame;
		torso->lerp = ent->animation1.lerp;
		torso->back_lerp = 1.0 - ent->animation1.lerp;
	}

	Cg_AnimateClientEntity_(ci->torso, &ent->animation2);

	if (legs) {
		legs->frame = ent->animation2.frame;
		legs->old_frame = ent->animation2.old_frame;
		legs->lerp = ent->animation2.lerp;
		legs->back_lerp = 1.0 - ent->animation2.lerp;
	}
}

/**
 * @brief The min velocity we should apply leg rotation on.
 */
#define CLIENT_LEGS_SPEED_EPSILON		.5f

/**
 * @brief The max yaw that we'll rotate the legs by when moving left/right.
 */
#define CLIENT_LEGS_YAW_MAX				65.f

/**
 * @brief Clamp angle
 */
#define CLIENT_LEGS_CLAMP				(CLIENT_LEGS_YAW_MAX * 1.5f)

/**
 * @brief The speed that the legs will catch up to the current leg yaw.
 */
#define CLIENT_LEGS_YAW_LERP_SPEED		240.f

/**
 * @brief
 */
static inline float AngleMod(float a) {
	a = fmodf(a, 360.f);// (360.0 / 65536) * ((int32_t) (a * (65536 / 360.0)) & 65535);

	if (a < 0) {
		return a + (((int32_t)(a / 360.f) + 1) * 360.f);
	}

	return a;
}

/**
 * @brief
 */
static inline float SmallestAngleBetween(const float x, const float y) {
	return min(360.f - fabsf(x - y), fabsf(x - y));
}

/**
 * @brief
 */
static inline float Cg_CalculateAngle(const float speed, float current, float ideal) {
	current = AngleMod(current);
	ideal = AngleMod(ideal);

	if (current == ideal) {
		return current;
	}

	float move = ideal - current;

	if (ideal > current) {
		if (move >= 180.0) {
			move = move - 360.0;
		}
	} else {
		if (move <= -180.0) {
			move = move + 360.0;
		}
	}

	if (move > 0) {
		if (move > speed) {
			move = speed;
		}
	} else {
		if (move < -speed) {
			move = -speed;
		}
	}

	return AngleMod(current + move);
}

/**
 * @brief Adds the numerous render entities which comprise a given client (player)
 * entity: head, torso, legs, weapon, flags, etc.
 */
void Cg_AddClientEntity(cl_entity_t *ent, r_entity_t *e) {
	const entity_state_t *s = &ent->current;

	const cl_client_info_t *ci = &cgi.client->client_info[s->client];
	if (!ci->head || !ci->torso || !ci->legs) {
		Cg_Debug("Invalid client info: %d\n", s->client);
		return;
	}

	if (Cg_IsSelf(ent)) {
		e->effects |= EF_SELF;
	}

	// don't draw ourselves unless third person is set
	if (Cg_IsSelf(ent) && !cgi.client->third_person) {

		e->effects |= EF_NO_DRAW;

		// keep our shadow underneath us using the predicted origin
		e->origin.x = cgi.view->origin.x;
		e->origin.y = cgi.view->origin.y;
	} else {
		Cg_BreathTrail(ent);
	}

	// set tints
	if (ci->shirt.a) {
		e->tints[0] = Color_Vec4(ci->shirt);
	}

	if (ci->pants.a) {
		e->tints[1] = Color_Vec4(ci->pants);
	}

	if (ci->helmet.a) {
		e->tints[2] = Color_Vec4(ci->helmet);
	}

	r_entity_t head, torso, legs;

	// copy the specified entity to all body segments
	head = torso = legs = *e;

	if ((ent->current.effects & EF_CORPSE) == 0) {
		vec3_t right;
		Vec3_Vectors(legs.angles, NULL, &right, NULL);

		vec3_t move_dir;
		move_dir = Vec3_Subtract(ent->prev.origin, ent->current.origin);
		move_dir.z = 0.f; // don't care about z, just x/y

		if (ent->animation2.animation < ANIM_LEGS_SWIM) {
			if (Vec3_Length(move_dir) > CLIENT_LEGS_SPEED_EPSILON) {
				move_dir = Vec3_Normalize(move_dir);
				float legs_yaw = Vec3_Dot(move_dir, right) * CLIENT_LEGS_YAW_MAX;

				if (ent->animation2.animation == ANIM_LEGS_BACK ||
					ent->animation2.reverse) {
					legs_yaw = -legs_yaw;
				}

				ent->legs_yaw = ent->angles.y + legs_yaw;
			} else {
				ent->legs_yaw = ent->angles.y;
			}
		} else {

			const float angle_diff = SmallestAngleBetween(ent->legs_yaw, ent->angles.y);

			if (ent->animation2.animation == ANIM_LEGS_TURN ||
				fabsf(angle_diff) > CLIENT_LEGS_YAW_MAX) {

				ent->legs_yaw = ent->angles.y;

				// change animation as well
				if (ent->animation2.animation == ANIM_LEGS_IDLE) {
					ent->animation2.time = cgi.client->unclamped_time;
					ent->animation2.frame = ent->animation2.old_frame = -1;
					ent->animation2.lerp = ent->animation2.fraction = 0;
				}
			}
		}

		ent->legs_current_yaw = Cg_CalculateAngle(CLIENT_LEGS_YAW_LERP_SPEED * MILLIS_TO_SECONDS(cgi.client->frame_msec), ent->legs_current_yaw, ent->legs_yaw);

		const float angle_delta = AngleMod(ent->legs_current_yaw - ent->legs_yaw + 180.0f) - 180.0f;

		ent->legs_current_yaw = AngleMod(ent->legs_yaw + clamp(angle_delta, -CLIENT_LEGS_CLAMP, CLIENT_LEGS_CLAMP));

		if (fabsf(SmallestAngleBetween(ent->legs_yaw, ent->legs_current_yaw)) > 1) {
			if (ent->animation2.animation == ANIM_LEGS_IDLE) {
				ent->animation2.time = cgi.client->unclamped_time;
				ent->animation2.animation = ANIM_LEGS_TURN;
			}
		} else {
			if (ent->animation2.animation == ANIM_LEGS_TURN) {
				ent->animation2.time = cgi.client->unclamped_time;
				ent->animation2.animation = ANIM_LEGS_IDLE;
			}
		}
	}

	legs.model = ci->legs;
	legs.angles.y = ent->legs_current_yaw;
	legs.angles.x = legs.angles.z = 0.0; // legs only use yaw
	memcpy(legs.skins, ci->legs_skins, sizeof(legs.skins));

	torso.model = ci->torso;
	torso.origin = Vec3_Zero();
	torso.angles.y = ent->angles.y - legs.angles.y; // legs twisted already, we just need to pitch/roll
	memcpy(torso.skins, ci->torso_skins, sizeof(torso.skins));

	head.model = ci->head;
	head.origin = Vec3_Zero();
	head.angles.y = 0.0;
	memcpy(head.skins, ci->head_skins, sizeof(head.skins));

	Cg_AnimateClientEntity(ent, &torso, &legs);

	r_entity_t *r_legs = cgi.AddEntity(cgi.view, &legs);
	if (!r_legs) {
		return; // if the legs were culled, we're done
	}

	torso.parent = r_legs;
	torso.tag = "tag_torso";

	r_entity_t *r_torso = cgi.AddEntity(cgi.view, &torso);
	assert(r_torso);

	head.parent = r_torso;
	head.tag = "tag_head";

	r_entity_t *r_head = cgi.AddEntity(cgi.view, &head);
	assert(r_head);

	if (s->model2) {
		cgi.AddEntity(cgi.view, &(const r_entity_t) {
			.parent = r_torso,
			.tag = "tag_weapon",
			.scale = e->scale,
			.model = cgi.client->models[s->model2],
			.effects = e->effects,
			.color = e->color,
			.shell = e->shell,
		});
	}

	if (s->model3) {
		cgi.AddEntity(cgi.view, &(const r_entity_t) {
			.parent = r_torso,
			.tag = "tag_head",
			.scale = e->scale,
			.model = cgi.client->models[s->model3],
			.effects = e->effects,
			.color = e->color,
			.shell = e->shell,
		});
	}

	if (s->model4) {
		cgi.Warn("Unsupported model_index4\n");
	}
}
