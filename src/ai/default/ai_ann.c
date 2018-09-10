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
	dvec3_t angles;
	dvec3_t velocity;
} ai_ann_input_t;

#define AI_ANN_INPUTS (sizeof(ai_ann_input_t) / sizeof(double))

typedef union {
	dvec3_t angles;
	dvec_t forward;
	dvec_t right;
	dvec_t up;
	dvec_t buttons;
} ai_ann_output_t;

#define AI_ANN_OUTPUTS (sizeof(ai_ann_output_t) / sizeof(double))

#define AI_ANN_HIDDEN_LAYERS 4
#define AI_ANN_NEURONS 4
#define AI_ANN_LEARNING_RATE 1.0

static genann *ai_ann;

/**
 * @brief
 */
void Ai_InitAnn(void) {

	ai_ann = genann_init(AI_ANN_INPUTS, AI_ANN_HIDDEN_LAYERS, AI_ANN_NEURONS, AI_ANN_OUTPUTS);
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
 * @brief
 */
void Ai_Learn(const g_entity_t *ent, const pm_cmd_t *cmd) {

	if (ent->client->ai) {
		return;
	}

	if (ent->client->ps.pm_state.type == PM_NORMAL) {
		if (cmd->forward || cmd->right) {

			ai_ann_input_t in;

			VectorCopy(ent->s.origin, in.origin);
			VectorCopy(ent->s.angles, in.angles);
			VectorCopy(ENTITY_DATA_ARRAY(ent, velocity), in.velocity);

			ai_ann_output_t out;

			VectorCopy(cmd->angles, out.angles);
			out.forward = cmd->forward;
			out.right = cmd->right;
			out.up = cmd->up;
			out.buttons = cmd->buttons;

			genann_train(ai_ann, (const dvec_t *) &in, (const dvec_t *) &out, AI_ANN_LEARNING_RATE);
		}
	}
}

/**
 * @brief
 */
void Ai_Predict(const g_entity_t *ent, pm_cmd_t *cmd) {

	ai_ann_input_t in;

	VectorCopy(ent->s.origin, in.origin);
	VectorCopy(ent->s.angles, in.angles);
	VectorCopy(ENTITY_DATA_ARRAY(ent, velocity), in.velocity);

	ai_ann_output_t *out = (ai_ann_output_t *) genann_run(ai_ann, (const dvec_t *) &in);
	assert(out);

	VectorCopy(out->angles, cmd->angles);
	cmd->forward = out->forward;
	cmd->right = out->right;
	cmd->up = out->up;
	cmd->buttons = out->buttons;
}
