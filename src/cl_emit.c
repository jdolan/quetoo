/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

/*
 * Emits are client-sided entities for emitting lights, particles, coronas,
 * ambient sounds, etc.  They are run once per frame.
 */

#define EMIT_LIGHT		0x1
#define EMIT_SPARKS		0x2
#define EMIT_STEAM		0x4
#define EMIT_FLAME		0x8
#define EMIT_CORONA		0x10
#define EMIT_SOUND		0x20
#define EMIT_MODEL		0x40

typedef struct emit_s {
	int flags;
	vec3_t org;
	vec3_t angles;  // for model orientation
	vec3_t dir;  // for particle direction
	vec3_t color;
	float hz, drift;  // how frequently and randomly we fire
	float radius;  // flame and corona radius
	vec3_t scale;  // mesh model scale
	int count;  // particle counts
	sfx_t *sfx;
	int attenuation;  // sound attenuation
	qboolean loop;  // loop sound versus timed
	model_t *mod;
	const mleaf_t *leaf;  // for pvs culling
	static_lighting_t lighting;  // cached static lighting info
	int time;  // when to fire next
} emit_t;

#define MAX_EMITS 512
static emit_t emits[MAX_EMITS];
static int num_emits = 0;


/*
 * Cl_LoadEmits
 *
 * Parse misc_emits from the bsp after it has been loaded.  This must
 * be called after Cm_LoadMap, once per pre-cache routine.
 */
void Cl_LoadEmits(void){
	const char *c, *ents;
	char class[128];
	emit_t *e;
	qboolean entity, emit;

	memset(&emits, 0, sizeof(emits));
	num_emits = 0;

	ents = Cm_EntityString();

	memset(class, 0, sizeof(class));
	entity = emit = false;

	e = NULL;

	while(true){
		c = Com_Parse(&ents);

		if(!strlen(c))
			break;

		if(*c == '{')
			entity = true;

		if(!entity)  // skip any whitespace between ents
			continue;

		if(*c == '}'){
			entity = false;

			Com_Dprintf("Closed entity %s\n", class);

			if(emit){

				// resolve the leaf so that the entity may be added only when in PVS
				e->leaf = R_LeafForPoint(e->org, r_worldmodel);

				// add default sounds and models
				if(!e->sfx){

					if(e->flags & EMIT_FLAME)  // fire crackling
						e->sfx = S_LoadSound("world/fire.wav");
				}

				// crutch up flags as a convenience
				if(e->sfx)
					e->flags |= EMIT_SOUND;

				if(e->mod)
					e->flags |= EMIT_MODEL;

				if(VectorCompare(e->color, vec3_origin))  // default color
					VectorSet(e->color, 1.0, 1.0, 1.0);

				if(!e->count)  // default particle count
					e->count = 12;

				if(!e->radius){  // default flame and corona radius

					if(e->flags & EMIT_CORONA)
						e->radius = 12.0;
					else if(e->flags & EMIT_LIGHT)
						e->radius = 1.5;
					else
						e->radius = 1.0;
				}

				if(VectorCompare(e->scale, vec3_origin)){  // default mesh model scale

					if(e->flags & EMIT_MODEL)
						VectorSet(e->scale, 1.0, 1.0, 1.0);
					else
						VectorSet(e->scale, 1.0, 1.0, 1.0);
				}

				if(!e->hz){  // default hz and drift

					if(e->flags & EMIT_FLAME)
						e->hz = 5.0;
					else if(e->flags & EMIT_SOUND)
						e->hz = 0.0;  // ambient
					else
						e->hz = 1.0;
				}

				if(!e->drift){
					e->drift = 0.01;
				}

				if(e->flags & EMIT_SOUND){  // resolve attenuation and looping

					if(e->attenuation == -1)  // explicit -1 for global
						e->attenuation = ATTN_NONE;
					else if(e->attenuation == 0)  // default
						e->attenuation = ATTN_NORM;

					if(e->flags & EMIT_FLAME)  // flame sounds are always loop
						e->loop = true;
					else  // the default is to honor the hz parameter
						e->loop = e->hz == 0.0;
				}

				e->lighting.dirty = true;

				Com_Dprintf("Added %d emit at %f %f %f\n", e->flags,
						e->org[0], e->org[1], e->org[2]);

				num_emits++;
			}
			else
				memset(&emits[num_emits], 0, sizeof(emit_t));

			emit = false;
		}

		if(!strcmp(c, "classname")){

			c = Com_Parse(&ents);
			strncpy(class, c, sizeof(class) - 1);

			if(!strcmp(c, "misc_emit"))
				emit = true;
		}

		e = &emits[num_emits];

		if(!strcmp(c, "flags")){
			e->flags = atoi(Com_Parse(&ents));
			continue;
		}

		if(!strcmp(c, "origin")){
			sscanf(Com_Parse(&ents), "%f %f %f", &e->org[0], &e->org[1], &e->org[2]);
			continue;
		}

		if(!strcmp(c, "angles")){  // resolve angles and directional vector
			sscanf(Com_Parse(&ents), "%f %f %f", &e->angles[0], &e->angles[1], &e->angles[2]);
			AngleVectors(e->angles, e->dir, NULL, NULL);
			continue;
		}

		if(!strcmp(c, "color")){  // resolve color as floats
			sscanf(Com_Parse(&ents), "%f %f %f", &e->color[0], &e->color[1], &e->color[2]);
			continue;
		}

		if(!strcmp(c, "hz")){
			e->hz = atof(Com_Parse(&ents));
			continue;
		}

		if(!strcmp(c, "drift")){
			e->drift = atof(Com_Parse(&ents));
			continue;
		}

		if(!strcmp(c, "radius")){
			e->radius = atof(Com_Parse(&ents));
			continue;
		}

		if(!strcmp(c, "scale")){
			sscanf(Com_Parse(&ents), "%f %f %f", &e->scale[0], &e->scale[1], &e->scale[2]);
			continue;
		}

		if(!strcmp(c, "count")){
			e->count = atoi(Com_Parse(&ents));
			continue;
		}

		if(!strcmp(c, "sound")){
			e->sfx = S_LoadSound(Com_Parse(&ents));
			if(e->sfx)
				e->sfx->cache = S_LoadSfx(e->sfx);
			continue;
		}

		if(!strcmp(c, "attenuation")){
			e->attenuation = atoi(Com_Parse(&ents));
			continue;
		}

		if(!strcmp(c, "model")){
			e->mod = R_LoadModel(va("models/%s/tris.md3", Com_Parse(&ents)));
			continue;
		}
	}
}


/*
 * Cl_AddEmits
 */
void Cl_AddEmits(void){
	int i;
	entity_t ent;

	if(!cl_emits->value)
		return;

	if(!num_emits)
		return;

	memset(&ent, 0, sizeof(ent));

	for(i = 0; i < num_emits; i++){

		emit_t *e = &emits[i];

		if(e->leaf && (e->leaf->visframe != r_locals.visframe))
			continue;  // culled

		if((e->flags & EMIT_SOUND) && e->loop)  // add an ambient sound
			S_AddLoopSound(e->org, e->sfx);

		if(e->flags & EMIT_MODEL){
			// fake a packet entity and add it to the view
			VectorCopy(e->org, ent.origin);
			VectorCopy(e->angles, ent.angles);

			ent.model = e->mod;
			ent.lerp = 1.0;

			VectorCopy(e->scale, ent.scale);

			ent.lighting = &e->lighting;

			R_AddEntity(&ent);
		}

		// most emits are timed events, so simply continue if it's
		// not time to fire our event yet

		if(e->time && (e->time > cl.time))
			continue;

		if(e->flags & EMIT_LIGHT)
			R_AddSustainedLight(e->org, e->radius, e->color, 0.65);

		if(e->flags & EMIT_SPARKS)
			Cl_SparksEffect(e->org, e->dir, e->count);

		if(e->flags & EMIT_STEAM)
			Cl_SmokeTrail(e->org, e->org, NULL);

		if(e->flags & EMIT_FLAME)
			Cl_FlameTrail(e->org, e->org, NULL);

		if(e->flags & EMIT_CORONA)
			R_AddCorona(e->org, e->radius, e->color);

		if((e->flags & EMIT_SOUND) && !e->loop)
			S_StartSound(e->org, 0, 0, e->sfx, 1, e->attenuation, 0);

		e->time = cl.time + (1000.0 / e->hz + (e->drift * frand()));
	}
}
