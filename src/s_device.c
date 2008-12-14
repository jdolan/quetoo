/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

#include <SDL.h>

#include "client.h"

extern dma_t dma;


/*
S_PaintAudio

Callback function given to SDL.
*/
static void S_PaintAudio(void *unused, Uint8 *stream, int len){

	dma.buffer = stream;
	dma.offset += len / (dma.bits / 4);

	S_PaintChannels(dma.offset);
}


/*
S_InitDevice
*/
qboolean S_InitDevice(void){
	SDL_AudioSpec desired, obtained;

	if(SDL_WasInit(SDL_INIT_AUDIO | SDL_INIT_VIDEO) == 0){
		if(SDL_Init(SDL_INIT_AUDIO) < 0){
			Com_Warn("S_InitDevice: %s\n", SDL_GetError());
			return false;
		}
	} else if(SDL_WasInit(SDL_INIT_AUDIO) == 0){
		if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0){
			Com_Warn("S_InitDevice: %s\n", SDL_GetError());
			return false;
		}
	}

	if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
		desired.format = AUDIO_S16MSB;
	else
		desired.format = AUDIO_S16LSB;

	desired.channels = (int)s_channels->value;
	if(desired.channels < 1)
		desired.channels = 1;
	else if(desired.channels > 2)
		desired.channels = 2;

	desired.freq = *s_rate->string ?  // casting the float is problematic
			atoi(s_rate->string) : 48000;

	if(desired.freq == 48000 || desired.freq == 44100)
		desired.samples = 4096;  // set buffer based on rate
	else if(desired.freq == 22050)
		desired.samples = 2048;
	else desired.samples = 1024;

	desired.callback = S_PaintAudio;

	if(SDL_OpenAudio(&desired, &obtained) < 0)  // open device
		return false;

	switch(obtained.format){  // ensure format is supported
		case AUDIO_S16LSB:
		case AUDIO_S16MSB:
			if(((obtained.format == AUDIO_S16LSB) &&
					(SDL_BYTEORDER == SDL_LIL_ENDIAN)) ||
					((obtained.format == AUDIO_S16MSB) &&
					(SDL_BYTEORDER == SDL_BIG_ENDIAN))){
				break;
			}
		default:  // unsupported format
			SDL_CloseAudio();
			return false;
	}
	SDL_PauseAudio(0);

	// copy obtained to dma struct
	dma.bits = (obtained.format & 0xFF);
	dma.rate = obtained.freq;
	dma.channels = obtained.channels;
	dma.samples = obtained.samples * dma.channels;
	dma.offset = 0;
	dma.chunk = 1;
	dma.buffer = NULL;

	return true;
}


/*
S_ShutdownDevice
*/
void S_ShutdownDevice(void){

	SDL_CloseAudio();

	if(SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
