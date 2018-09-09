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

#include "genann.h"

#define AI_ANN_LAYERS 4
#define AI_ANN_NEURONS 64

static genann *ai_ann;

/**
 * @brief
 */
void Ai_InitAnn(void) {

	ai_ann = genann_init(AI_ANN_INPUTS, AI_ANN_LAYERS, AI_ANN_NEURONS, AI_ANN_OUTPUTS);
	assert(ai_ann);
}

/**
 * @brief
 */
void Ai_ShutdownAnn(void) {

	genann_free(ai_ann);
	ai_ann = NULL;
}

/**
 * @brief
 */
void Ai_TrainAnn(const g_entity_t *ent, const pm_cmd_t *cmd) {

}

/**
 *
 */
void Ai_PredictAnn(const g_entity_t *ent, pm_cmd_t *cmd) {

}
