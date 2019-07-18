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

#define __CG_LOCAL_H__

#define Debug(...) Debug_(__func__, __VA_ARGS__)
#define Error(...) Error_(__func__, __VA_ARGS__)
#define Warn(...) Warn_(__func__, __VA_ARGS__)

#include "cg_client.h"
#include "cg_effect.h"
#include "cg_emit.h"
#include "cg_entity.h"
#include "cg_entity_effect.h"
#include "cg_entity_event.h"
#include "cg_entity_trail.h"
#include "cg_hud.h"
#include "cg_input.h"
#include "cg_main.h"
#include "cg_media.h"
#include "cg_muzzle_flash.h"
#include "cg_particle.h"
#include "cg_predict.h"
#include "cg_score.h"
#include "cg_stain.h"
#include "cg_temp_entity.h"
#include "cg_types.h"
#include "cg_ui.h"
#include "cg_view.h"
