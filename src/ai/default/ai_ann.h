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

#pragma once

#include "ai_types.h"

#ifdef __AI_LOCAL_H__

typedef struct {
	dvec3_t origin;
	dvec3_t velocity;
	dvec_t node;

} ai_ann_input_t;

#define AI_ANN_INPUTS (sizeof(ai_ann_input_t) / sizeof(double))

typedef union {
	dvec_t forward;
	dvec_t right;
	dvec_t up;

} ai_ann_output_t;

#define AI_ANN_OUTPUTS (sizeof(ai_ann_output_t) / sizeof(double))

void Ai_InitAnn(void);
void Ai_ShutdownAnn(void);
void Ai_TrainAnn(const g_entity_t *ent, const pm_cmd_t *cmd);
void Ai_PredictAnn(const g_entity_t *ent, pm_cmd_t *cmd);
#endif
