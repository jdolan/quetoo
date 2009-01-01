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

#include "client.h"

static byte *data_p;
static byte *iff_end;
static byte *last_chunk;
static byte *iff_data;
static int iff_chunk_len;


/*
 * S_GetLittleShort
 */
static short S_GetLittleShort(void){
	short val = 0;
	val = *data_p;
	val = val +(*(data_p + 1) << 8);
	data_p += 2;
	return val;
}


/*
 * S_GetLittleLong
 */
static int S_GetLittleLong(void){
	int val = 0;
	val = *data_p;
	val = val +(*(data_p + 1) << 8);
	val = val +(*(data_p + 2) << 16);
	val = val +(*(data_p + 3) << 24);
	data_p += 4;
	return val;
}


/*
 * S_FindNextChunk
 */
static void S_FindNextChunk(char *name){
	while(true){
		data_p = last_chunk;

		if(data_p >= iff_end){  // didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = S_GetLittleLong();
		if(iff_chunk_len < 0){
			data_p = NULL;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 +((iff_chunk_len + 1) & ~1);
		if(!strncmp((char *)data_p, name, 4))
			return;
	}
}


/*
 * S_FindChunk
 */
static void S_FindChunk(char *name){
	last_chunk = iff_data;
	S_FindNextChunk(name);
}


/*
 * S_GetWavinfo
 */
static wavinfo_t S_GetWavinfo(const char *name, byte *wav, int wavlength){
	wavinfo_t info;
	int i;
	int format;
	int samples;

	memset(&info, 0, sizeof(info));

	if(!wav)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	S_FindChunk("RIFF");
	if(!(data_p && !strncmp((char *)data_p + 8, "WAVE", 4))){
		Com_Warn("S_GetWavinfo: Missing RIFF/WAVE chunks.\n");
		return info;
	}

	// get "fmt " chunk
	iff_data = data_p + 12;

	S_FindChunk("fmt ");
	if(!data_p){
		Com_Warn("S_GetWavinfo: Missing fmt chunk.\n");
		return info;
	}
	data_p += 8;
	format = S_GetLittleShort();
	if(format != 1){
		Com_Warn("S_GetWavinfo: Not PCM format.\n");
		return info;
	}

	info.channels = S_GetLittleShort();
	info.rate = S_GetLittleLong();
	data_p += 4 + 2;
	info.width = S_GetLittleShort() / 8;

	// get cue chunk
	S_FindChunk("cue ");
	if(data_p){
		data_p += 32;
		info.loopstart = S_GetLittleLong();

		// if the next chunk is a LIST chunk, look for a cue length marker
		S_FindNextChunk("LIST");
		if(data_p){
			if(!strncmp((char *)data_p + 28, "mark", 4)){  // this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = S_GetLittleLong();  // samples in loop
				info.samples = info.loopstart + i;
			}
		}
	} else
		info.loopstart = -1;

	// find data chunk
	S_FindChunk("data");
	if(!data_p){
		Com_Warn("S_GetWavinfo: %s is missing data chunk.\n", name);
		return info;
	}

	data_p += 4;
	samples = S_GetLittleLong() / info.width;

	if(info.samples){
		if(samples < info.samples){
			Com_Warn("S_GetWavinfo: %s has a bad loop length.\n", name);
			info.samples = 0;
		}
	} else
		info.samples = samples;

	info.dataofs = data_p - wav;

	return info;
}


/*
 * S_ResampleSfx
 */
static void S_ResampleSfx(sfx_t *sfx, int inrate, int inwidth, byte *data){
	int outcount;
	int srcsample;
	float stepscale;
	int i;
	int sample, samplefrac, fracstep;
	sfxcache_t *sc;

	sc = sfx->cache;
	if(!sc)
		return;

	stepscale = (float)inrate / dma.rate;  // this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if(sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->rate = dma.rate;
	sc->width = inwidth;
	sc->stereo = 0;

	// resample / decimate to the current source rate

	if(stepscale == 1 && inwidth == 1 && sc->width == 1){
		// fast special case
		for(i = 0; i < outcount; i++)
			((signed char *)sc->data)[i]
			= (int)((unsigned char)(data[i]) - 128);
	} else {
		// general case
		samplefrac = 0;
		fracstep = stepscale * 256;
		for(i = 0; i < outcount; i++){
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if(inwidth == 2)
				sample = LittleShort(((short *)data)[srcsample]);
			else
				sample =(int)((unsigned char)(data[srcsample]) - 128) << 8;
			if(sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}


/*
 * S_LoadSfx
 */
sfxcache_t *S_LoadSfx(sfx_t *s){
	char *c, name[MAX_QPATH];
	void *buf;
	byte *data;
	wavinfo_t info;
	int len;
	float stepscale;
	sfxcache_t *sc;
	int size;

	if(!s || s->name[0] == '*')
		return NULL;

	// see if still in memory
	sc = s->cache;
	if(sc)
		return sc;

	// load it in
	if(s->truename)
		c = s->truename;
	else
		c = s->name;

	memset(name, 0, sizeof(name));

	if(*c == '#')
		strncpy(name, (c + 1), sizeof(name) - 1);
	else
		snprintf(name, sizeof(name), "sounds/%s", c);

	if(!strstr(name, ".wav"))  // append suffix
		strcat(name, ".wav");

	if((size = Fs_LoadFile(name, &buf)) == -1){
		//Com_Warn("S_LoadSfx: Couldn't load %s.\n", name);
		return NULL;
	}

	data = (byte *)buf;

	info = S_GetWavinfo(name, data, size);
	if(info.rate == 0 || info.samples == 0 || info.channels == 0){
		Com_Warn("%s is not a valid wav file.\n", name);
		Fs_FreeFile(buf);
		return NULL;
	}

	if(info.channels != 1){
		Com_Warn("%s is a stereo sample.\n", name);
		Fs_FreeFile(buf);
		return NULL;
	}

	stepscale = (float)info.rate / dma.rate;
	len = info.samples / stepscale;

	len = len * info.width * info.channels;

	sc = s->cache = Z_Malloc(len + sizeof(*sc));
	if(!sc){
		Fs_FreeFile(buf);
		return NULL;
	}

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->rate = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	S_ResampleSfx(s, sc->rate, sc->width, data + info.dataofs);

	Fs_FreeFile(buf);
	return sc;
}

