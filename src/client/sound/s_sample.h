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

s_sample_t *S_LoadSample(const char *name);
s_sample_t *S_LoadClientModelSample(const char *model, const char *name);

#ifdef __S_LOCAL_H__
size_t S_Resample(const int32_t channels, const int32_t source_rate, const int32_t dest_rate, const size_t num_frames, const int16_t *in_frames, int16_t **out_frames, size_t *out_size);
void S_ConvertSamples(const float *input_samples, const sf_count_t num_samples, int16_t **out_samples, size_t *out_size);
#endif /* __S_LOCAL_H__ */
