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

#include "common.h"

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

typedef struct {
	vec3_t origin;  // full float precision
	vec3_t velocity;  // full float precision

	vec3_t forward, right, up;
	float frametime;

	csurface_t *groundsurface;
	cplane_t groundplane;
	int groundcontents;

	vec3_t previous_origin;
	qboolean ladder;
} pml_t;

pmove_t *pm;
pml_t pml;


// movement parameters
float pm_maxspeed = 300.0;
float pm_duckspeed = 150.0;
float pm_jumpspeed = 300.0;
float pm_accelerate = 10.0;
float pm_airaccelerate = 2.0;

float pm_friction = 6.0;
float pm_stopspeed = 100.0;

float pm_waterspeed = 150.0;
float pm_wateraccelerate = 6.0;
float pm_waterfriction = 1.0;
float pm_waterjump = 350.0;

float pm_specspeed = 400.0;
float pm_specaccelerate = 3.5;
float pm_specfriction = 3.0;


/*
 * Pm_ClipVelocity
 *
 * Slide off of the impacting object
 * returns the blocked flags (1 = floor, 2 = step / wall)
 */
static void Pm_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce){
	float backoff;
	float change;
	int i;

	backoff = DotProduct(in, normal) * overbounce;

	if(backoff < 0.0)
		backoff *= overbounce;
	else
		backoff /= overbounce;

	for(i = 0; i < 3; i++){
		change = normal[i] * backoff;
		out[i] = in[i] - change;
	}
}


/*
 * Pm_StepSlideMove
 *
 * Each intersection will try to step over the obstruction instead of
 * sliding along it.
 *
 * Returns a new origin, velocity, and contact entity
 * Does not modify any world state?
 */
#define MIN_STEP_NORMAL	0.7  // can't step up onto very steep slopes
#define MAX_CLIP_PLANES	5
static void Pm_StepSlideMove_(void){
	int bumpcount, numbumps;
	vec3_t dir;
	float d;
	int numplanes;
	vec3_t planes[MAX_CLIP_PLANES];
	vec3_t primal_velocity;
	int i, j;
	trace_t trace;
	vec3_t end;
	float time_left;

	numbumps = 4;

	VectorCopy(pml.velocity, primal_velocity);
	numplanes = 0;

	time_left = pml.frametime;

	for(bumpcount = 0; bumpcount < numbumps; bumpcount++){
		for(i = 0; i < 3; i++)
			end[i] = pml.origin[i] + time_left * pml.velocity[i];

		trace = pm->trace(pml.origin, pm->mins, pm->maxs, end);

		if(trace.allsolid){  // entity is trapped in another solid
			pml.velocity[2] = 0;  // don't build up falling damage
			return;
		}

		if(trace.fraction > 0){  // actually covered some distance
			VectorCopy(trace.endpos, pml.origin);
			numplanes = 0;
		}

		if(trace.fraction == 1)
			break;  // moved the entire distance

		// save entity for contact
		if(pm->numtouch < MAXTOUCH && trace.ent){
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}

		time_left -= time_left * trace.fraction;

		// slide along this plane
		if(numplanes >= MAX_CLIP_PLANES){  // this shouldn't really happen
			VectorCopy(vec3_origin, pml.velocity);
			break;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for(i = 0; i < numplanes; i++){
			Pm_ClipVelocity(pml.velocity, planes[i], pml.velocity, 1.001);
			for(j = 0; j < numplanes; j++)
				if(j != i){
					if(DotProduct(pml.velocity, planes[j]) < 0)
						break;  // not ok
				}
			if(j == numplanes)
				break;
		}

		if(i != numplanes){  // go along this plane
		} else {  // go along the crease
			if(numplanes != 2){
				VectorCopy(vec3_origin, pml.velocity);
				break;
			}
			CrossProduct(planes[0], planes[1], dir);
			d = DotProduct(dir, pml.velocity);
			VectorScale(dir, d, pml.velocity);
		}

		// if velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if(DotProduct(pml.velocity, primal_velocity) <= 0){
			VectorCopy(vec3_origin, pml.velocity);
			break;
		}
	}

	if(pm->s.pm_time){
		VectorCopy(primal_velocity, pml.velocity);
	}
}


/*
 * Pm_StepSlideMove
 */
static void Pm_StepSlideMove(void){
	vec3_t start_o, start_v;
	vec3_t down_o, down_v;
	trace_t trace;
	float down_dist, up_dist;
	vec3_t up, down;

	VectorCopy(pml.origin, start_o);
	VectorCopy(pml.velocity, start_v);

	Pm_StepSlideMove_();

	VectorCopy(pml.origin, down_o);
	VectorCopy(pml.velocity, down_v);

	VectorCopy(start_o, up);
	up[2] += PM_STAIR_HEIGHT;

	trace = pm->trace(up, pm->mins, pm->maxs, up);
	if(trace.allsolid)
		return;  // can't step up

	// try sliding above
	VectorCopy(up, pml.origin);
	VectorCopy(start_v, pml.velocity);

	Pm_StepSlideMove_();

	// push down the final amount
	VectorCopy(pml.origin, down);
	down[2] -= PM_STAIR_HEIGHT;
	trace = pm->trace(pml.origin, pm->mins, pm->maxs, down);
	if(!trace.allsolid){
		VectorCopy(trace.endpos, pml.origin);
	}

	VectorCopy(pml.origin, up);

	// decide which one went farther
	down_dist = (down_o[0] - start_o[0]) * (down_o[0] - start_o[0]) + 
				(down_o[1] - start_o[1]) * (down_o[1] - start_o[1]);
	up_dist = (up[0] - start_o[0]) * (up[0] - start_o[0]) + 
			  (up[1] - start_o[1]) * (up[1] - start_o[1]);

	if(down_dist > up_dist || trace.plane.normal[2] < MIN_STEP_NORMAL){
		VectorCopy(down_o, pml.origin);
		VectorCopy(down_v, pml.velocity);
		return;
	}
	//!! Special case
	// if we were walking along a plane, then we need to copy the Z over
	pml.velocity[2] = down_v[2];
}


/*
 * Pm_Friction
 *
 * Handles both ground friction and water friction
 */
static void Pm_Friction(void){
	float *vel;
	float speed, newspeed, control;
	float friction;
	float drop;

	vel = pml.velocity;

	speed = VectorLength(vel);
	if(speed < 1){
		vel[0] = 0;
		vel[1] = 0;
		return;
	}

	drop = 0;

	// apply ground friction
	if((pm->groundentity && pml.groundsurface && !(pml.groundsurface->flags & SURF_SLICK)) || (pml.ladder)){
		friction = pm_friction;
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * friction * pml.frametime;
	}

	// apply water friction
	if(pm->waterlevel && !pml.ladder)
		drop += speed * pm_waterfriction * pm->waterlevel * pml.frametime;

	if(pm->s.pm_type == PM_SPECTATOR)
		drop += speed * pm_specfriction * pml.frametime;

	// scale the velocity
	newspeed = speed - drop;
	if(newspeed < 0)
		VectorClear(vel);
	else {
		newspeed /= speed;
		VectorScale(vel, newspeed, vel);
	}
}


/*
 * Pm_Accelerate
 *
 * Handles user intended acceleration
 */
static void Pm_Accelerate(vec3_t wishdir, float wishspeed, float accel){
	int i;
	float addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct(pml.velocity, wishdir);
	addspeed = wishspeed - currentspeed;

	if(addspeed <= 0)
		return;

	accelspeed = accel * pml.frametime * wishspeed;

	if(accelspeed > addspeed)
		accelspeed = addspeed;

	for(i = 0; i < 3; i++)
		pml.velocity[i] += accelspeed * wishdir[i];
}


/*
 * Pm_AddCurrents
 */
static void Pm_AddCurrents(vec3_t wishvel){
	vec3_t v;
	float s;

	// account for ladders
	if(pml.ladder && fabsf(pml.velocity[2]) <= 200){
		if((pm->angles[PITCH] <= -15) && (pm->cmd.forwardmove > 0))
			wishvel[2] = 200;
		else if((pm->angles[PITCH] >= 15) && (pm->cmd.forwardmove > 0))
			wishvel[2] = -200;
		else if(pm->cmd.upmove > 0)
			wishvel[2] = 200;
		else if(pm->cmd.upmove < 0)
			wishvel[2] = -200;
		else
			wishvel[2] = 0;

		// limit horizontal speed when on a ladder
		if(wishvel[0] < -25)
			wishvel[0] = -25;
		else if(wishvel[0] > 25)
			wishvel[0] = 25;

		if(wishvel[1] < -25)
			wishvel[1] = -25;
		else if(wishvel[1] > 25)
			wishvel[1] = 25;
	}

	// add water currents
	if(pm->watertype & MASK_CURRENT){
		VectorClear(v);

		if(pm->watertype & CONTENTS_CURRENT_0)
			v[0] += 1;
		if(pm->watertype & CONTENTS_CURRENT_90)
			v[1] += 1;
		if(pm->watertype & CONTENTS_CURRENT_180)
			v[0] -= 1;
		if(pm->watertype & CONTENTS_CURRENT_270)
			v[1] -= 1;
		if(pm->watertype & CONTENTS_CURRENT_UP)
			v[2] += 1;
		if(pm->watertype & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		s = pm_maxspeed;
		if((pm->waterlevel == 1) && (pm->groundentity))
			s = pm_waterspeed;

		VectorMA(wishvel, s, v, wishvel);
	}

	// add conveyor belt velocities
	if(pm->groundentity){
		VectorClear(v);

		if(pml.groundcontents & CONTENTS_CURRENT_0)
			v[0] += 1;
		if(pml.groundcontents & CONTENTS_CURRENT_90)
			v[1] += 1;
		if(pml.groundcontents & CONTENTS_CURRENT_180)
			v[0] -= 1;
		if(pml.groundcontents & CONTENTS_CURRENT_270)
			v[1] -= 1;
		if(pml.groundcontents & CONTENTS_CURRENT_UP)
			v[2] += 1;
		if(pml.groundcontents & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		VectorMA(wishvel, 100 /* pm->groundentity->speed */, v, wishvel);
	}
}


/*
 * Pm_WaterMove
 */
static void Pm_WaterMove(void){
	int i;
	vec3_t wishvel;
	float wishspeed;
	vec3_t wishdir;

	// user intentions
	for(i = 0; i < 3; i++)
		wishvel[i] = pml.forward[i] * pm->cmd.forwardmove + pml.right[i] * pm->cmd.sidemove;

	if(!pm->cmd.forwardmove && !pm->cmd.sidemove && !pm->cmd.upmove)
		wishvel[2] -= 60;  // sink
	else
		wishvel[2] += pm->cmd.upmove;

	Pm_AddCurrents(wishvel);

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if(wishspeed > pm_waterspeed){
		VectorScale(wishvel, pm_waterspeed / wishspeed, wishvel);
		wishspeed = pm_waterspeed;
	}

	Pm_Accelerate(wishdir, wishspeed, pm_wateraccelerate);

	Pm_StepSlideMove();
}


/*
 * Pm_AirMove
 */
static void Pm_AirMove(void){
	int i;
	vec3_t wishvel;
	float fmove, smove;
	vec3_t wishdir;
	float wishspeed;
	float maxspeed;

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	for(i = 0; i < 2; i++)
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] = 0;

	Pm_AddCurrents(wishvel);

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// clamp to server defined max speed
	maxspeed = (pm->s.pm_flags & PMF_DUCKED) ? pm_duckspeed : pm_maxspeed;
	
	// account for speed modulus
	maxspeed = (pm->cmd.buttons & BUTTON_WALK ? maxspeed * 0.66 : maxspeed);

	if(wishspeed > maxspeed){
		VectorScale(wishvel, maxspeed / wishspeed, wishvel);
		wishspeed = maxspeed;
	}

	if(pml.ladder){
		Pm_Accelerate(wishdir, wishspeed, pm_accelerate);
		if(!wishvel[2]){
			if(pml.velocity[2] > 0){
				pml.velocity[2] -= pm->s.gravity * pml.frametime;
				if(pml.velocity[2] < 0)
					pml.velocity[2] = 0;
			} else {
				pml.velocity[2] += pm->s.gravity * pml.frametime;
				if(pml.velocity[2] > 0)
					pml.velocity[2] = 0;
			}
		}
		Pm_StepSlideMove();
	}
	else if(pm->groundentity){  // walking on ground
		pml.velocity[2] = 0;  //!!! this is before the accel
		Pm_Accelerate(wishdir, wishspeed, pm_accelerate);

		if(pm->s.gravity > 0)
			pml.velocity[2] = 0;
		else
			pml.velocity[2] -= pm->s.gravity * pml.frametime;

		if(!pml.velocity[0] && !pml.velocity[1])
			return;

		Pm_StepSlideMove();
	}
	else {  // not on ground, so little effect on velocity
		Pm_Accelerate(wishdir, wishspeed, pm_airaccelerate);
		// add gravity
		pml.velocity[2] -= pm->s.gravity * pml.frametime;
		Pm_StepSlideMove();
	}
}


/*
 * Pm_CategorizePosition
 */
static void Pm_CategorizePosition(void){
	vec3_t point;
	int cont;
	trace_t trace;
	int sample1;
	int sample2;

	// if the player hull point one unit down is solid, the player
	// is on ground

	// see if standing on something solid
	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] - 0.15;

	if(pml.velocity[2] > 220){  // cant jump
		pm->s.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = NULL;
	} else {
		trace = pm->trace(pml.origin, pm->mins, pm->maxs, point);
		pml.groundplane = trace.plane;
		pml.groundsurface = trace.surface;
		pml.groundcontents = trace.contents;

		if(!trace.ent || (trace.plane.normal[2] < MIN_STEP_NORMAL && !trace.startsolid)){
			pm->groundentity = NULL;
			pm->s.pm_flags &= ~PMF_ON_GROUND;
		} else {
			pm->groundentity = trace.ent;

			// hitting solid ground will end a waterjump
			if(pm->s.pm_flags & PMF_TIME_WATERJUMP){
				pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
				pm->s.pm_time = 0;
			}

			if(!(pm->s.pm_flags & PMF_ON_GROUND)){  // just hit the ground
				pm->s.pm_flags |= PMF_ON_GROUND;
				// don't do landing time if we were just going down a slope
				if(pml.velocity[2] < -400){
					pm->s.pm_flags |= PMF_TIME_LAND;
					// don't allow another jump for a little while
					if(pml.velocity[2] < -600)
						pm->s.pm_time = 20;
					else pm->s.pm_time = 4;
				}
			}
		}

		if(pm->numtouch < MAXTOUCH && trace.ent){
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
	}

	// get waterlevel, accounting for ducking
	pm->waterlevel = 0;
	pm->watertype = 0;

	sample2 = pm->viewheight - pm->mins[2];
	sample1 = sample2 / 2;

	point[2] = pml.origin[2] + pm->mins[2] + 1;
	cont = pm->pointcontents(point);

	if(cont & MASK_WATER){
		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = pml.origin[2] + pm->mins[2] + sample1;
		cont = pm->pointcontents(point);
		if(cont & MASK_WATER){
			pm->waterlevel = 2;
			point[2] = pml.origin[2] + pm->mins[2] + sample2;
			cont = pm->pointcontents(point);
			if(cont & MASK_WATER)
				pm->waterlevel = 3;
		}
	}
}


/*
 * Pm_CheckJump
 */
static void Pm_CheckJump(void){
	if(pm->s.pm_flags & PMF_TIME_LAND){  // hasn't been long enough since landing to jump again
		return;
	}

	if(pm->cmd.upmove < 10){  // not holding jump
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// must wait for jump to be released
	if(pm->s.pm_flags & PMF_JUMP_HELD)
		return;

	if(pm->s.pm_type == PM_DEAD)
		return;

	if(pm->waterlevel >= 2){  // swimming, not jumping
		pm->groundentity = NULL;

		if(pml.velocity[2] <= -pm_jumpspeed)
			return;

		if(pm->watertype == CONTENTS_WATER)
			pml.velocity[2] = pm_jumpspeed / 3.0;
		else if(pm->watertype == CONTENTS_SLIME)
			pml.velocity[2] = pm_jumpspeed / 4.0;
		else
			pml.velocity[2] = pm_jumpspeed / 6.0;
		return;
	}

	if(pm->groundentity == NULL)
		return;  // in air, so no effect

	pm->s.pm_flags |= PMF_JUMP_HELD;
	pm->groundentity = NULL;

	pml.velocity[2] += pm_jumpspeed;

	if(pml.velocity[2] < pm_jumpspeed)
		pml.velocity[2] = pm_jumpspeed;
}


/*
 * Pm_CheckSpecialMovement
 */
static void Pm_CheckSpecialMovement(void){
	vec3_t spot;
	int cont;
	vec3_t flatforward;
	trace_t trace;

	if(pm->s.pm_time)
		return;

	pml.ladder = false;

	// check for ladder
	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalize(flatforward);

	VectorMA(pml.origin, 1, flatforward, spot);
	trace = pm->trace(pml.origin, pm->mins, pm->maxs, spot);
	if((trace.fraction < 1) && (trace.contents & CONTENTS_LADDER))
		pml.ladder = true;

	// check for water jump
	if(pm->waterlevel != 2)
		return;

	VectorMA(pml.origin, 30, flatforward, spot);
	spot[2] += 4;
	cont = pm->pointcontents(spot);
	if(!(cont & CONTENTS_SOLID))
		return;

	spot[2] += PM_STAIR_HEIGHT;
	cont = pm->pointcontents(spot);
	if(cont)
		return;

	// jump out of water
	VectorScale(flatforward, 50, pml.velocity);
	pml.velocity[2] = pm_waterjump;

	pm->s.pm_flags |= PMF_TIME_WATERJUMP;
	pm->s.pm_time = 255;
}


/*
 * Pm_CheckDuck
 *
 * Sets mins, maxs, and pm->viewheight
 */
static void Pm_CheckDuck(void){
	trace_t trace;

	pm->mins[0] = -16;
	pm->mins[1] = -16;

	pm->maxs[0] = 16;
	pm->maxs[1] = 16;

	pm->mins[2] = -24;

	if(pm->s.pm_type == PM_DEAD){
		pm->s.pm_flags |= PMF_DUCKED;
	} else if(pm->cmd.upmove < 0 &&(pm->s.pm_flags & PMF_ON_GROUND)){  // duck
		pm->s.pm_flags |= PMF_DUCKED;
	} else {  // stand up if possible
		if(pm->s.pm_flags & PMF_DUCKED){
			// try to stand up
			pm->maxs[2] = 32;
			trace = pm->trace(pml.origin, pm->mins, pm->maxs, pml.origin);
			if(!trace.allsolid)
				pm->s.pm_flags &= ~PMF_DUCKED;
		}
	}

	if(pm->s.pm_flags & PMF_DUCKED){
		pm->maxs[2] = 4;
		pm->viewheight = -2;
	} else {
		pm->maxs[2] = 32;
		pm->viewheight = 22;
	}
}


/*
 * Pm_GoodPosition
 */
static qboolean Pm_GoodPosition(void){
	trace_t trace;
	vec3_t origin, end;
	int i;

	if(pm->s.pm_type == PM_SPECTATOR)
		return true;

	for(i = 0; i < 3; i++)
		origin[i] = end[i] = pm->s.origin[i] * 0.125;

	trace = pm->trace(origin, pm->mins, pm->maxs, end);

	return !trace.allsolid;
}


/*
 * Pm_SnapPosition
 *  
 * On exit, the origin will have a value that is pre-quantized to the 0.125
 * precision of the network channel and in a valid position.
 */
static void Pm_SnapPosition(void){
	int sign[3];
	int i, j, bits;
	short base[3];
	// try all single bits first
	static int jitterbits[8] = {0, 4, 1, 2, 3, 5, 6, 7};

	// snap velocity to eigths
	for(i = 0; i < 3; i++)
		pm->s.velocity[i] = (int)(pml.velocity[i] * 8);

	for(i = 0; i < 3; i++){
		if(pml.origin[i] >= 0)
			sign[i] = 1;
		else
			sign[i] = -1;
		pm->s.origin[i] = (int)(pml.origin[i] * 8);
		if(pm->s.origin[i] * 0.125 == pml.origin[i])
			sign[i] = 0;
	}
	VectorCopy(pm->s.origin, base);

	// try all combinations
	for(j = 0; j < 8; j++){
		bits = jitterbits[j];
		VectorCopy(base, pm->s.origin);
		for(i = 0; i < 3; i++)
			if(bits & (1 << i))
				pm->s.origin[i] += sign[i];

		if(Pm_GoodPosition())
			return;
	}

	// go back to the last position
	VectorCopy(pml.previous_origin, pm->s.origin);
}


/*
 * Pm_InitialSnapPosition
 */
static void Pm_InitialSnapPosition(void){
	int x, y, z;
	short base[3];
	static int offset[3] = { 0, -1, 1 };

	VectorCopy(pm->s.origin, base);

	for(z = 0; z < 3; z++){
		pm->s.origin[2] = base[2] + offset[ z ];
		for(y = 0; y < 3; y++){
			pm->s.origin[1] = base[1] + offset[ y ];
			for(x = 0; x < 3; x++){
				pm->s.origin[0] = base[0] + offset[ x ];
				if(Pm_GoodPosition()){
					pml.origin[0] = pm->s.origin[0] * 0.125;
					pml.origin[1] = pm->s.origin[1] * 0.125;
					pml.origin[2] = pm->s.origin[2] * 0.125;
					VectorCopy(pm->s.origin, pml.previous_origin);
					return;
				}
			}
		}
	}
	Com_Dprintf("Bad InitialSnapPosition\n");
}


/*
 * Pm_ClampAngles
 */
static void Pm_ClampAngles(void){
	short	temp;
	int i;

	if(pm->s.pm_flags & PMF_TIME_TELEPORT){
		pm->angles[YAW] = SHORT2ANGLE(pm->cmd.angles[YAW] + pm->s.delta_angles[YAW]);
		pm->angles[PITCH] = 0;
		pm->angles[ROLL] = 0;
	} else {
		// circularly clamp the angles with deltas
		for(i = 0; i < 3; i++){
			temp = pm->cmd.angles[i] + pm->s.delta_angles[i];
			pm->angles[i] = SHORT2ANGLE(temp);
		}

		// don't let the player look up or down more than 90 degrees
		if(pm->angles[PITCH] > 89 && pm->angles[PITCH] < 180)
			pm->angles[PITCH] = 89;
		else if(pm->angles[PITCH] < 271 && pm->angles[PITCH] >= 180)
			pm->angles[PITCH] = 271;
	}
	AngleVectors(pm->angles, pml.forward, pml.right, pml.up);
}


/*
 * Pm_SpectatorMove
 */
static void Pm_SpectatorMove(){
	vec3_t wishvel, wishdir;
	float fmove, smove, wishspeed;
	int i;

	pm->viewheight = 22;

	// apply friction
	Pm_Friction();

	// resolve desired movement
	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	for(i = 0; i < 3; i++)
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] += pm->cmd.upmove;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// clamp to max speed
	if(wishspeed > pm_specspeed)
		wishspeed = pm_specspeed;

	// accelerate
	Pm_Accelerate(wishdir, wishspeed, pm_specaccelerate);

	// do the move
	for(i = 0; i < 3; i++)
		pml.origin[i] = pml.origin[i] + pml.frametime * pml.velocity[i];
}


/*
 * Pmove
 *
 * Can be called by either the server or the client to update prediction.
 */
void Pmove(pmove_t *pmove){
	pm = pmove;

	// clear results
	pm->numtouch = 0;
	VectorClear(pm->angles);
	pm->viewheight = 0;
	pm->groundentity = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;

	// clear all pmove local vars
	memset(&pml, 0, sizeof(pml));

	// convert origin and velocity to float values
	pml.origin[0] = pm->s.origin[0] * 0.125;
	pml.origin[1] = pm->s.origin[1] * 0.125;
	pml.origin[2] = pm->s.origin[2] * 0.125;

	pml.velocity[0] = pm->s.velocity[0] * 0.125;
	pml.velocity[1] = pm->s.velocity[1] * 0.125;
	pml.velocity[2] = pm->s.velocity[2] * 0.125;

	// save old org in case we get stuck
	VectorCopy(pm->s.origin, pml.previous_origin);

	pml.frametime = pm->cmd.msec * 0.001;

	Pm_ClampAngles();

	if(pm->s.pm_type == PM_SPECTATOR){
		Pm_SpectatorMove();
		Pm_SnapPosition();
		return;
	}

	if(pm->s.pm_type >= PM_DEAD){
		pm->cmd.forwardmove = 0;
		pm->cmd.sidemove = 0;
		pm->cmd.upmove = 0;
	}

	if(pm->s.pm_type == PM_FREEZE)
		return;  // no movement at all

	// set mins, maxs, and viewheight
	Pm_CheckDuck();

	if(pm->snapinitial)
		Pm_InitialSnapPosition();

	// set groundentity, watertype, and waterlevel
	Pm_CategorizePosition();

	Pm_CheckSpecialMovement();

	// drop timing counter
	if(pm->s.pm_time){
		int msec;

		msec = pm->cmd.msec >> 3;
		if(!msec)
			msec = 1;
		if(msec >= pm->s.pm_time){
			pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->s.pm_time = 0;
		} else
			pm->s.pm_time -= msec;
	}

	if(pm->s.pm_flags & PMF_TIME_TELEPORT){  // teleport pause stays exactly in place
	} else if(pm->s.pm_flags & PMF_TIME_WATERJUMP){  // waterjump has no control, but falls
		pml.velocity[2] -= pm->s.gravity * pml.frametime;
		if(pml.velocity[2] < 0){  // cancel as soon as we are falling down again
			pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->s.pm_time = 0;
		}

		Pm_StepSlideMove();
	} else {
		Pm_CheckJump();

		Pm_Friction();

		if(pm->waterlevel >= 2)
			Pm_WaterMove();
		else {
			vec3_t angles;

			VectorCopy(pm->angles, angles);
			if(angles[PITCH] > 180)
				angles[PITCH] = angles[PITCH] - 360;
			angles[PITCH] /= 3;

			AngleVectors(angles, pml.forward, pml.right, pml.up);

			Pm_AirMove();
		}
	}

	// set groundentity, watertype, and waterlevel for final spot
	Pm_CategorizePosition();

	Pm_SnapPosition();
}

