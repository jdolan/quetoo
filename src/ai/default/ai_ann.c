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

#include "ai_local.h"

#include "deps/genann/genann.h"

typedef struct {
    dvec3_t origin;
	dvec3_t velocity;
} ai_ann_input_t;

#define AI_ANN_INPUTS (sizeof(ai_ann_input_t) / sizeof(double))

typedef struct {
	dvec3_t dir;
} ai_ann_output_t;

#define AI_ANN_OUTPUTS (sizeof(ai_ann_output_t) / sizeof(double))

#define AI_ANN_HIDDEN_LAYERS 1
#define AI_ANN_NEURONS 64
#define AI_ANN_LEARNING_RATE 1.0

static genann *ai_ann;

/**
 * @brief
 */
void Ai_InitAnn(void) {

	ai_ann = genann_init(AI_ANN_INPUTS, AI_ANN_HIDDEN_LAYERS, AI_ANN_NEURONS, AI_ANN_OUTPUTS);
    genann_randomize(ai_ann);
	assert(ai_ann);
}

/**
 * @brief
 */
void Ai_ShutdownAnn(void) {

	FILE *file = fopen("/tmp/quetoo.ann", "w");
	genann_write(ai_ann, file);
	fclose(file);

	genann_free(ai_ann);
	ai_ann = NULL;
}

/**
 * @brief Trains the neural network with a movement command from a human client.
 * @param ent The client entity.
 * @param cmd The movement command issued by the client.
 */
void Ai_Learn(const g_entity_t *ent, const pm_cmd_t *cmd) {

	if (ent->client->ai) {
		return;
	}

	if (ent->client->ps.pm_state.type == PM_NORMAL) {
		if (cmd->forward || cmd->right || cmd->up) {
			vec3_t angles, forward, right, up, dir;

			ai_ann_input_t in;

            VectorCopy(ent->s.origin, in.origin);
			VectorCopy(ENTITY_DATA_ARRAY(ent, velocity), in.velocity);

			ai_ann_output_t out;

			UnpackAngles(cmd->angles, angles);
			AngleVectors(angles, forward, right, up);

			VectorClear(dir);
			VectorMA(dir, cmd->forward, forward, dir);
			VectorMA(dir, cmd->right, right, dir);
			VectorMA(dir, cmd->up, up, dir);

			VectorNormalize(dir);
			VectorCopy(dir, out.dir);

			genann_train(ai_ann, (const dvec_t *) &in, (const dvec_t *) &out, AI_ANN_LEARNING_RATE);
		}
	}
}

/**
 * @brief
 */
void Ai_Predict(const g_entity_t *ent, vec3_t dir) {

	ai_ann_input_t in;

	VectorCopy(ent->s.origin, in.origin);
	VectorCopy(ENTITY_DATA_ARRAY(ent, velocity), in.velocity);

	const ai_ann_output_t *out = (ai_ann_output_t *) genann_run(ai_ann, (const dvec_t *) &in);
	assert(out);

	VectorCopy(out->dir, dir);
	VectorNormalize(dir);
}
