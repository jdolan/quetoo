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

typedef struct {
	int left;
	int right;
} portable_samplepair_t;

#define PAINTBUFFER_SIZE 2048
static portable_samplepair_t s_paintbuffer[PAINTBUFFER_SIZE];

#define MAX_RAW_SAMPLES	8192
static portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

static int snd_scaletable[32][256];
static int *snd_p, snd_linear_count, snd_vol;

static short *snd_out;


/*
 * S_WriteLinearBlastStereo16
 */
static void S_WriteLinearBlastStereo16(void){
	int i;
	int val;

	for(i = 0; i < snd_linear_count; i += 2){
		val = snd_p[i] >> 8;
		if(val > 0x7ddd)
			snd_out[i] = 0x7ddd;
		else if(val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = snd_p[i + 1] >> 8;
		if(val > 0x7ddd)
			snd_out[i + 1] = 0x7ddd;
		else if(val < (short)0x8000)
			snd_out[i + 1] = (short)0x8000;
		else
			snd_out[i + 1] = val;
	}
}


/*
 * S_TransferStereo16
 */
static void S_TransferStereo16(unsigned long *pbuf, int endtime){
	int paintedtime;

	snd_p = (int *)s_paintbuffer;
	paintedtime = s_env.paintedtime;

	while(paintedtime < endtime){
		// handle recirculating buffer issues
		const int lpos = paintedtime &((s_env.device.samples >> 1) - 1);

		snd_out = (short *)pbuf + (lpos << 1);

		snd_linear_count = (s_env.device.samples >> 1) - lpos;
		if(paintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - paintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		S_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		paintedtime +=(snd_linear_count >> 1);
	}
}


/*
 * S_TransferPaintBuffer
 */
static void S_TransferPaintBuffer(int endtime){
	int out_idx, out_mask;
	int i, count;
	int *p;
	int step, val;
	unsigned long *pbuf;

	pbuf = (unsigned long *)s_env.device.buffer;

	if(s_testsound->value){  // write a fixed sine wave
		count = (endtime - s_env.paintedtime);
		for(i = 0; i < count; i++)
			s_paintbuffer[i].left = s_paintbuffer[i].right =
				sin((s_env.paintedtime + i) * 0.1) * 20000 * 256;
	}

	if(s_env.device.bits == 16 && s_env.device.channels == 2){  // optimized case
		S_TransferStereo16(pbuf, endtime);
		return;
	}

	p = (int *)s_paintbuffer;  // general case
	count = (endtime - s_env.paintedtime) * s_env.device.channels;
	out_mask = s_env.device.samples - 1;
	out_idx = s_env.paintedtime * s_env.device.channels & out_mask;
	step = 3 - s_env.device.channels;

	if(s_env.device.bits == 16){
		short *out = (short *)pbuf;
		while(count--){
			val = *p >> 8;
			p += step;
			if(val > 0x7fff)
				val = 0x7fff;
			else if(val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	} else if(s_env.device.bits == 8){
		unsigned char *out = (unsigned char *)pbuf;
		while(count--){
			val = *p >> 8;
			p += step;
			if(val > 0x7fff)
				val = 0x7fff;
			else if(val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = (val >> 8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
}


void S_PaintChannelFrom8(s_channel_t *ch, s_samplecache_t *sc, int endtime, int offset);
void S_PaintChannelFrom16(s_channel_t *ch, s_samplecache_t *sc, int endtime, int offset);

/*
 * S_PaintChannels
 */
void S_PaintChannels(int endtime){
	int i;
	int end;
	s_channel_t *ch;
	s_samplecache_t *sc;
	int ltime, count;
	s_sampleplay_t *ps;

	while(s_env.paintedtime < endtime){
		// if s_paintbuffer is smaller than DMA buffer
		end = endtime;
		if(endtime - s_env.paintedtime > PAINTBUFFER_SIZE)
			end = s_env.paintedtime + PAINTBUFFER_SIZE;

		i = 0;
		// start any s_env.sampleplays
		while(true){
			ps = s_env.pendingplays.next;
			if(ps == &s_env.pendingplays)
				break;  // no more pending sounds

			if(ps->begin <= s_env.paintedtime){
				S_IssueSampleplay(ps);
				continue;
			}

			if(ps->begin < end)
				end = ps->begin;  // stop here
			break;
		}

		// clear the paint buffer
		if(s_env.rawend < s_env.paintedtime){
			memset(s_paintbuffer, 0, (end - s_env.paintedtime) * sizeof(portable_samplepair_t));
		} else {  // copy from the streaming sound source
			int s;
			int stop;

			stop = (end < s_env.rawend) ? end : s_env.rawend;

			for(i = s_env.paintedtime; i < stop; i++){
				s = i & (MAX_RAW_SAMPLES - 1);
				s_paintbuffer[i - s_env.paintedtime] = s_rawsamples[s];
			}

			for(; i < end; i++){
				s_paintbuffer[i - s_env.paintedtime].left =
					s_paintbuffer[i - s_env.paintedtime].right = 0;
			}
		}

		// paint in the channels.
		ch = s_env.channels;
		for(i = 0; i < MAX_CHANNELS; i++, ch++){
			ltime = s_env.paintedtime;

			while(ltime < end){

				if(!ch->sample || (!ch->leftvol && !ch->rightvol))
					break;

				// max painting is to the end of the buffer
				count = end - ltime;

				// might be stopped by running out of data
				if(ch->end - ltime < count)
					count = ch->end - ltime;

				sc = S_LoadSamplecache(ch->sample);
				if(!sc)
					break;

				if(count > 0 && ch->sample){
					if(sc->width == 1)
						S_PaintChannelFrom8(ch, sc, count, ltime - s_env.paintedtime);
					else
						S_PaintChannelFrom16(ch, sc, count, ltime - s_env.paintedtime);

					ltime += count;
				}

				// if at end of loop, restart
				if(ltime >= ch->end){
					if(ch->autosound){  // autolooping sounds always go back to start
						ch->pos = 0;
						ch->end = ltime + sc->length;
					} else if(sc->loopstart >= 0){
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					} else {  // channel just stopped
						ch->sample = NULL;
					}
				}
			}
		}

		// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		s_env.paintedtime = end;
	}
}


/*
 * S_InitScaletable
 */
void S_InitScaletable(void){
	int i, j;
	int scale;

	if(s_volume->value > 1)  //cap volume
		s_volume->value = 1;

	for(i = 0; i < 32; i++){
		scale = i * 8 * 256 * s_volume->value;
		for(j = 0; j < 256; j++)
			snd_scaletable[i][j] = (signed char)j * scale;
	}

	snd_vol = s_volume->value * 256;
	s_volume->modified = false;
}


/*
 * S_PaintChannelFrom8
 */
void S_PaintChannelFrom8(s_channel_t *ch, s_samplecache_t *sc, int count, int offset){
	int data;
	int *lscale, *rscale;
	byte *sample;
	int i;
	portable_samplepair_t *samp;

	if(ch->leftvol > 255)
		ch->leftvol = 255;
	if(ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ ch->leftvol >> 3];
	rscale = snd_scaletable[ ch->rightvol >> 3];
	sample = (byte *)sc->data + ch->pos;

	samp = &s_paintbuffer[offset];

	for(i = 0; i < count; i++, samp++){
		data = sample[i];
		samp->left += lscale[data];
		samp->right += rscale[data];
	}

	ch->pos += count;
}


/*
 * S_PaintChannelFrom16
 */
void S_PaintChannelFrom16(s_channel_t *ch, s_samplecache_t *sc, int count, int offset){
	int data;
	int left, right;
	int leftvol, rightvol;
	signed short *sample;
	int i, j;
	portable_samplepair_t *samp;

	leftvol = ch->leftvol * snd_vol;
	rightvol = ch->rightvol * snd_vol;

	sample = (signed short *)sc->data + ch->pos;
	j = ch->pos;

	samp = &s_paintbuffer[offset];

	for(i = 0; i < count; i++, samp++){

		left = right = 0;

		if(j < sc->length){  // mix as long as we have sample data
			data = sample[i];

			left = (data * leftvol) >> 8;
			right = (data * rightvol) >> 8;

			j++;
		}
		else {
			Com_Warn("S_PaintChannelFrom16: Underrun.\n");
		}

		samp->left += left;
		samp->right += right;
	}

	ch->pos += count;
}
