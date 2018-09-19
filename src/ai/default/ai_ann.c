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
#define AI_ANN_LEARNING_RATE 0.666

static genann *ai_genann;

/**
 * @brief Trains the neural network with a movement command from a human client.
 * @param ent The client entity.
 * @param cmd The movement command issued by the client.
 */
void Ai_Learn(const g_entity_t *ent, const pm_cmd_t *cmd) {

	if (!ai_genann) {
		return;
	}

	if (ent->client->ai) {
		return;
	}

	if (ent->client->ps.pm_state.type == PM_NORMAL) {
		if (cmd->forward || cmd->right || cmd->up) {
			vec3_t angles, forward, right, up, dir;

			ai_ann_input_t in;

            VectorCopy(ent->s.origin, in.origin);
			VectorCopy(&ENTITY_DATA(ent, velocity), in.velocity);

			ai_ann_output_t out;

			UnpackAngles(cmd->angles, angles);
			AngleVectors(angles, forward, right, up);

			VectorClear(dir);
			VectorMA(dir, cmd->forward, forward, dir);
			VectorMA(dir, cmd->right, right, dir);
			VectorMA(dir, cmd->up, up, dir);

			VectorNormalize(dir);
			VectorCopy(dir, out.dir);

			genann_train(ai_genann, (const dvec_t *) &in, (const dvec_t *) &out, AI_ANN_LEARNING_RATE);
		}
	}
}

/**
 * @brief
 */
void Ai_Predict(const g_entity_t *ent, vec3_t dir) {

	if (!ai_genann) {
		VectorClear(dir);
		return;
	}

	ai_ann_input_t in;

	VectorCopy(ent->s.origin, in.origin);
	VectorCopy(&ENTITY_DATA(ent, velocity), in.velocity);

	const ai_ann_output_t *out = (ai_ann_output_t *) genann_run(ai_genann, (const dvec_t *) &in);
	assert(out);

	VectorCopy(out->dir, dir);
	VectorNormalize(dir);
}

/**
 * @brief
 */
void Ai_InitAnn(void) {

	if (ai_ann->value) {
		ai_ann->modified = false;

		const char *path = aim.gi->RealPath("ai/genann.ann");
		FILE *file = fopen(path, "r");
		if (file) {
			ai_genann = genann_read(file);
			if (ai_genann) {
				if (ai_genann->inputs == AI_ANN_INPUTS &&
					ai_genann->outputs == AI_ANN_OUTPUTS &&
					ai_genann->hidden_layers == AI_ANN_HIDDEN_LAYERS &&
					ai_genann->hidden == AI_ANN_NEURONS) {
					aim.gi->Print("  Loaded %s, %d weights\n", path, ai_genann->total_weights);
				} else {
					genann_free(ai_genann);
					ai_genann = NULL;
					aim.gi->Warn("Failed to load %s (wrong format)\n", path);
				}
			} else {
				aim.gi->Warn("Failed to load %s (invalid)\n", path);
			}
			fclose(file);
		}

		if (ai_genann == NULL) {
			ai_genann = genann_init(AI_ANN_INPUTS, AI_ANN_HIDDEN_LAYERS, AI_ANN_NEURONS, AI_ANN_OUTPUTS);
			assert(ai_genann);
		}

		aim.gi->Print("  Neural net: ^2%zd inputs, %zd outputs, ^2%d layers, %d neurons\n",
					  AI_ANN_INPUTS, AI_ANN_OUTPUTS, AI_ANN_HIDDEN_LAYERS, AI_ANN_NEURONS);
	}
}

/**
 * @brief
 */
void Ai_ShutdownAnn(void) {

	if (ai_genann) {
		const char *path = aim.gi->RealPath("ai/genann.ann");
		FILE *file = fopen(path, "w");
		if (file) {

			genann_write(ai_genann, file);
			fclose(file);

			aim.gi->Debug("Saved %s\n", path);
		}

		genann_free(ai_genann);
		ai_genann = NULL;
	}
}
