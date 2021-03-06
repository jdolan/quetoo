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

#ifndef __SOUND_H__

#include "common/common.h"

#include "s_main.h"
#include "s_media.h"
#include "s_mix.h"
#include "s_music.h"
#include "s_sample.h"
#include "s_types.h"

extern s_context_t s_context;

extern cvar_t *s_ambient_volume;
extern cvar_t *s_doppler;
extern cvar_t *s_effects;
extern cvar_t *s_effects_volume;
extern cvar_t *s_rate;
extern cvar_t *s_volume;

#endif /* __SOUND_H__ */
