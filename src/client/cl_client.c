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

#include "client.h"

#define DEFAULT_CLIENT_INFO "newbie\\qforcer/enforcer"


/*
 * Cl_ValidateClientInfo
 */
static qboolean Cl_ValidateClientInfo(cl_client_info_t *ci){

	if(!ci->head || !ci->upper || !ci->lower)
		return false;

	if(ci->head_skin == r_null_image || ci->upper_skin == r_null_image ||
			ci->lower_skin == r_null_image){
		return false;
	}

	return true;
}


/*
 * Cl_LoadClientInfo
 */
void Cl_LoadClientInfo(cl_client_info_t *ci, const char *s){
	char model_name[MAX_QPATH];
	char skin_name[MAX_QPATH];
	char path[MAX_QPATH];
	const char *t;
	char *u, *v;
	int i;

	// copy the entire string
	strncpy(ci->info, s, sizeof(ci->info));
	ci->info[sizeof(ci->info) - 1] = '\0';

	i = 0;
	t = s;
	while(*t){  // check for non-printable chars
		if(*t <= 32){
			i = -1;
			break;
		}
		t++;
	}

	if(*ci->info == '\0' || i == -1){  // use default
		Cl_LoadClientInfo(ci, DEFAULT_CLIENT_INFO);
		return;
	}

	// isolate the player's name
	strncpy(ci->name, s, sizeof(ci->name));
	ci->name[sizeof(ci->name) - 1] = 0;

	v = strchr(ci->name, '\\');
	u = strchr(ci->name, '/');

	if(v && u && (v < u)){  // valid
		*v = *u = 0;
		strcpy(model_name, v + 1);
		strcpy(skin_name, u + 1);
	}
	else {  // invalid
		Cl_LoadClientInfo(ci, DEFAULT_CLIENT_INFO);
		return;
	}

	// load the models
	snprintf(path, sizeof(path), "players/%s/head.md3", model_name);
	ci->head = R_LoadModel(path);

	snprintf(path, sizeof(path), "players/%s/upper.md3", model_name);
	ci->upper = R_LoadModel(path);

	snprintf(path, sizeof(path), "players/%s/lower.md3", model_name);
	ci->lower = R_LoadModel(path);

	// and the skins
	snprintf(path, sizeof(path), "players/%s/%s_h.tga", model_name, skin_name);
	ci->head_skin = R_LoadImage(path, it_skin);

	snprintf(path, sizeof(path), "players/%s/%s_u.tga", model_name, skin_name);
	ci->upper_skin = R_LoadImage(path, it_skin);

	snprintf(path, sizeof(path), "players/%s/%s_l.tga", model_name, skin_name);
	ci->lower_skin = R_LoadImage(path, it_skin);

	// ensure we were able to load everything
	if(!Cl_ValidateClientInfo(ci)){
		Cl_LoadClientInfo(ci, DEFAULT_CLIENT_INFO);
		return;
	}

	Com_Debug("Loaded cl_client_info_t: %s\n", ci->info);
}


/*
 * Cl_AnimateClientEntity_
 *
 * Returns the appropriate frame number for the specified client animation, or
 * -1 if the current animation sequence is complete.
 */
static int Cl_AnimateClientEntity_(cl_entity_animation_t *a,
		const r_md3_animation_t *anim, float *lerp){

	if(!anim->num_frames || !anim->hz){  // bad animation sequence
		*lerp = 1.0;
		return -1;
	}

	const int frame_time = 1000 / anim->hz;
	const int duration = anim->num_frames * frame_time;
	const int time = cl.time - a->time;
	const int frame = time / frame_time;

	a->fraction = time / (float)duration;

	*lerp = (time % frame_time) / (float)frame_time;

	if(time > duration){  // to loop, or not to loop

		if(!anim->looped_frames){
			*lerp = 1.0;
			return -1;
		}

		return anim->first_frame + (frame % anim->looped_frames);
	}

	return anim->first_frame + (frame % anim->num_frames);
}


/*
 * Cl_AnimateClientEntity
 *
 * Runs the animation sequences for the specified entity, setting the frame
 * indexes and interpolation fractions for the specified renderer entities.
 */
void Cl_AnimateClientEntity(cl_entity_t *e, r_entity_t *upper, r_entity_t *lower){
	const r_md3_t *md3 = (r_md3_t *)upper->model->extra_data;
	const entity_state_t *s = &e->current;

	// set sane defaults, although we should never need them
	upper->frame = upper->old_frame = lower->frame = lower->old_frame = 0;
	upper->lerp = lower->lerp = 1.0;
	upper->back_lerp = lower->back_lerp = 0.0;

	if((s->animation1 & ~ANIM_TOGGLE_BIT) > md3->num_animations){
		Com_Warn("Cl_AnimateClientEntity: Invalid animation1: %s: %d\n",
				upper->model->name, s->animation1 & ~ANIM_TOGGLE_BIT);
		return;
	}

	if((s->animation2 & ~ANIM_TOGGLE_BIT) > md3->num_animations){
		Com_Warn("Cl_AnimateClientEntity: Invalid animation2: %s: %d\n",
				upper->model->name, s->animation2 & ~ANIM_TOGGLE_BIT);
		return;
	}

	const r_md3_animation_t *a1 = &md3->animations[s->animation1 & ~ANIM_TOGGLE_BIT];
	const r_md3_animation_t *a2 = &md3->animations[s->animation2 & ~ANIM_TOGGLE_BIT];

	int frame;
	float lerp;

	// do the upper body animation
	if(s->animation1 != e->prev.animation1){
		e->animation1.animation = s->animation1 & ~ANIM_TOGGLE_BIT;
		e->animation1.time = cl.time;
	}

	frame = Cl_AnimateClientEntity_(&e->animation1, a1, &lerp);

	if(frame != e->animation1.frame){

		if(frame == -1){  // revert to standing
			e->animation1.old_frame = md3->animations[ANIM_TORSO_STAND1].first_frame;
			e->animation1.frame = e->animation1.old_frame;
		}
		else {
			e->animation1.old_frame = e->animation1.frame;
			e->animation1.frame = frame;
		}
	}

	upper->frame = e->animation1.frame;
	upper->old_frame = e->animation1.old_frame;
	upper->lerp = lerp;
	upper->back_lerp = 1.0 - lerp;

	// and then the legs
	if(s->animation2 != e->prev.animation2){
		e->animation2.animation = s->animation2 & ~ANIM_TOGGLE_BIT;
		e->animation2.time = cl.time;
	}

	frame = Cl_AnimateClientEntity_(&e->animation2, a2, &lerp);

	if(frame != e->animation2.frame){

		if(frame == -1){  // revert to standing
			e->animation2.old_frame = md3->animations[ANIM_LEGS_IDLE].first_frame;
			e->animation2.frame = e->animation2.old_frame;
		}
		else {
			e->animation2.old_frame = e->animation2.frame;
			e->animation2.frame = frame;
		}
	}

	lower->frame = e->animation2.frame;
	lower->old_frame = e->animation2.old_frame;
	lower->lerp = lerp;
	lower->back_lerp = 1.0 - lerp;
}
