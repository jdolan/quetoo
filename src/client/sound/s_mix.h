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

#ifndef __S_MIX_H__
#define __S_MIX_H__

void S_LoopSample(const vec3_t org, s_sample_t *sample);
void S_PlaySample(const vec3_t org, int ent_num, s_sample_t *sample, int atten);
void S_StartLocalSample(const char *name);

#ifdef __S_LOCAL_H__

void S_FreeChannel(int c);
void S_SpatializeChannel(s_channel_t *channel);

#endif /* __S_LOCAL_H__ */

#endif /* __S_MIX_H__ */
