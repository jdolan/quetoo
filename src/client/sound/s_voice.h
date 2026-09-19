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

extern Cvar *s_voice;
extern Cvar *s_voice_bitrate;
extern Cvar *s_capture_gain;
extern Cvar *s_capture_normalize;
extern Cvar *s_voice_loopback;
extern Cvar *s_voice_volume;

void S_StartVoice(uint8_t channel);
void S_StopVoice(void);
void S_StopVoices(void);
int32_t S_ReadVoice(byte *data, uint8_t *seq, uint8_t *flags, uint8_t *channel);
void S_AddVoice(int32_t client, uint8_t seq, uint8_t flags, const byte *data, int32_t len);

#if defined(__S_LOCAL_H__)
void S_InitVoice(void);
void S_ShutdownVoice(void);

#endif
