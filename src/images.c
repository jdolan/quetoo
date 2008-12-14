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

#include <SDL_image.h>
#include <png.h>

#include "images.h"

// 8bit palette for wal images and particles
#define PALETTE "pics/colormap"
unsigned palette[256];
qboolean palette_initialized = false;

// .wal file header for loading legacy .wal textures
typedef struct miptex_s {
	char name[32];
	unsigned width, height;
	unsigned offsets[4];  // four mip maps stored
	char animname[32];  // next frame in animation chain
	int flags;
	int contents;
	int value;
} miptex_t;

// default pixel format to which all images are converted
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000

SDL_PixelFormat format = {
	NULL,  // palette
	32,  // bits
	4,  // bytes
	0,  // rloss
	0,  // gloss
	0,  // bloss
	0,  // aloss
	0,  // rshift
	8,  // gshift
	16,  // bshift
	24,  // ashift
	RMASK,  // rmask
	GMASK,  // gmask
	BMASK,  // bmask
	AMASK,  // amask
	0,  // colorkey
	1   // alpha
};


// image formats, tried in this order
char *TYPES[] = {
	"tga", "png", "jpg", "wal", "pcx", NULL
};

static inline void Img_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream){
	if (fwrite(ptr, size, nmemb, stream) <= 0)
	    Com_Printf( "Failed to write\n");
}

/*
Img_LoadImage

Loads the specified image from the game filesystem and populates
the provided SDL_Surface.

Image formats are tried in the order they appear in TYPES.
*/
qboolean Img_LoadImage(char *name, SDL_Surface **surf){
	int i;

	i = 0;
	while(TYPES[i]){
		if(Img_LoadTypedImage(name, TYPES[i++], surf))
			return true;
	}

	return false;
}


/*
Img_LoadWal

A helper which mangles a .wal file into an SDL_Surface suitable for
OpenGL uploads and other basic manipulations.
*/
static qboolean Img_LoadWal(char *path, SDL_Surface **surf){
	void *buf;
	miptex_t *mt;
	int i, size;
	unsigned *p;
	byte *b;

	if(Fs_LoadFile(path, &buf) == -1)
		return false;

	mt = (miptex_t *)buf;

	mt->width = LittleLong(mt->width);
	mt->height = LittleLong(mt->height);

	mt->offsets[0] = LittleLong(mt->offsets[0]);

	if(!palette_initialized)  // lazy-load palette if necessary
		Img_InitPalette();

	size = mt->width * mt->height;
	p = (unsigned *)malloc(size * sizeof(unsigned));

	b = (byte *)mt + mt->offsets[0];

	for(i = 0; i < size; i++){  // convert to 32bpp RGBA via palette
		if(b[i] == 255)  // transparent
			p[i] = 0;
		else
			p[i] = palette[b[i]];
	}

	// create the RGBA surface
	if(!(*surf = SDL_CreateRGBSurfaceFrom((void *)p, mt->width, mt->height,
			32, 0, RMASK, GMASK, BMASK, AMASK))){

		Fs_FreeFile(mt);
		return false;
	}

	// trick SDL into freeing the pixel data with the surface
	(*surf)->flags &= ~SDL_PREALLOC;

	Fs_FreeFile(mt);
	return true;
}


/*
Img_LoadTypedImage

Loads the specified image from the game filesystem and populates
the provided SDL_Surface.
*/
qboolean Img_LoadTypedImage(char *name, char *type, SDL_Surface **surf){
	char path[MAX_QPATH];
	void *buf;
	int len;
	SDL_RWops *rw;
	SDL_Surface *s;

	snprintf(path, sizeof(path), "%s.%s", name, type);

	if(!strcmp(type, "wal"))  // special case for .wal files
		return Img_LoadWal(path, surf);

	if((len = Fs_LoadFile(path, &buf)) == -1)
		return false;

	if(!(rw = SDL_RWFromMem(buf, len))){
		Fs_FreeFile(buf);
		return false;
	}

	if(!(*surf = IMG_LoadTyped_RW(rw, 0, type))){
		SDL_FreeRW(rw);
		Fs_FreeFile(buf);
		return false;
	}

	SDL_FreeRW(rw);
	Fs_FreeFile(buf);

	if(strstr(path, PALETTE))  // dont convert the palette
		return true;

	if(!(s = SDL_ConvertSurface(*surf, &format, 0))){
		SDL_FreeSurface(*surf);
		return false;
	}

	SDL_FreeSurface(*surf);
	*surf = s;

	return true;
}


/*
Img_InitPalette

Initializes the 8bit color palette required for .wal texture loading.
*/
void Img_InitPalette(void){
	SDL_Surface *surf;
	byte r, g, b;
	unsigned v;
	int i;

	if(!Img_LoadTypedImage(PALETTE, "pcx", &surf))
		return;

	for(i = 0; i < 256; i++){
		r = surf->format->palette->colors[i].r;
		g = surf->format->palette->colors[i].g;
		b = surf->format->palette->colors[i].b;

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		palette[i] = LittleLong(v);
	}

	palette[255] &= LittleLong(0xffffff);  // 255 is transparent

	SDL_FreeSurface(surf);

	palette_initialized = true;
}


/*
Img_ColorFromPalette

Returns RGB components of the specified color in the specified result array.
*/
void Img_ColorFromPalette(byte c, float *res){
	unsigned color;

	if(!palette_initialized)  // lazy-load palette if necessary
		Img_InitPalette();

	color = palette[c];

	res[0] = (color >>  0 & 255) / 255.0;
	res[1] = (color >>  8 & 255) / 255.0;
	res[2] = (color >> 16 & 255) / 255.0;
}


/*
Img_WriteTGARLE

Write pixel data to a Type 10 (RLE compressed RGB) Targa file.
*/
void Img_WriteTGARLE(char *path, void *data, int width, int height){
	FILE *tga_file;
	const unsigned int channels = 3;		// 24-bit RGB
	unsigned char header[18];
	// write image data
	// TGA has the R and B channels switched
	unsigned char pixel_data[channels];
	unsigned char block_data[channels*128];
	unsigned char rle_packet;
	unsigned char *img_data = (unsigned char*)data;
	int compress = 0;
	size_t block_length = 0;
	char footer[26];
	int y;
	size_t x;

	if(!(tga_file = fopen(path, "wb"))){  // failed to open
		Com_Printf( "Failed to open to %s\n", path);
		return;
	}

	// write TGA header
	memset(header, 0, sizeof(header));

	// byte 0       - image ID field length         = 0 (no image ID field present)
	// byte 1       - color map type                = 0 (no palette present)
	// byte 2       - image type                    = 10 (truecolor RLE encoded)
	header[2] = 10;
	// byte 3-11    - palette data (not used)
	// byte 12+13   - image width
	header[12] = (width & 0xff);
	header[13] = ((width >> 8) & 0xff);
	// byte 14+15   - image height
	header[14] = (height & 0xff);
	header[15] = ((height >> 8) & 0xff);
	// byte 16      - image color depth             = 24 (RGB) or 32 (RGBA)
	header[16] = channels * 8;
	// byte 17      - image descriptor byte		= 0x20 (origin at bottom left)
	header[17] = 0x20;

	// write header
	Img_fwrite((char *)header, 1, sizeof(header), tga_file);

	for(y = height - 1; y >= 0; y--){
		for(x = 0; x < width; x++){
			size_t index = y * width * channels + x * channels;
			// TGA has it channels in a different order
			pixel_data[0] = img_data[index + 2];
			pixel_data[1] = img_data[index + 1];
			pixel_data[2] = img_data[index];

			if (block_length == 0){
				memcpy(block_data, pixel_data, channels);
				block_length++;
				compress = 0;
			} else {
				if(!compress){
					// uncompressed block and pixel_data differs from the last pixel
					if(memcmp(&block_data[(block_length - 1) * channels], pixel_data, channels) != 0){
						// append pixel
						memcpy(&block_data[block_length * channels], pixel_data, channels);

						block_length++;
					} else {
						// uncompressed block and pixel data is identical
						if(block_length > 1){
							// write the uncompressed block
							rle_packet = block_length - 2;
							Img_fwrite(&rle_packet, 1, 1, tga_file);
							Img_fwrite(block_data, 1, (block_length - 1) * channels, tga_file);
							block_length = 1;
						}
						memcpy(block_data, pixel_data, channels);
						block_length++;
						compress = 1;
					}
				} else {
					// compressed block and pixel data is identical
					if(memcmp(block_data, pixel_data, channels) == 0){
						block_length++;
					} else {
						// compressed block and pixel data differs
						if(block_length > 1){
							// write the compressed block
							rle_packet = block_length + 127;
							Img_fwrite(&rle_packet, 1, 1, tga_file);
							Img_fwrite(block_data, 1, channels, tga_file);
							block_length = 0;
						}
						memcpy(&block_data[block_length * channels], pixel_data, channels);
						block_length++;
						compress = 0;
					}
				}
			}

			if(block_length == 128){
				rle_packet = block_length - 1;
				if(!compress){
					Img_fwrite(&rle_packet, 1, 1, tga_file);
					Img_fwrite(block_data, 1, 128 * channels, tga_file);
				} else {
					rle_packet += 128;
					Img_fwrite(&rle_packet, 1, 1, tga_file);
					Img_fwrite(block_data, 1, channels, tga_file);
				}

				block_length = 0;
				compress = 0;
			}
		}
	}

	// write remaining bytes
	if(block_length){
		rle_packet = block_length - 1;
		if (!compress) {
			Img_fwrite(&rle_packet, 1, 1, tga_file);
			Img_fwrite(block_data, 1, block_length * channels, tga_file);
		} else {
			rle_packet += 128;
			Img_fwrite(&rle_packet, 1, 1, tga_file);
			Img_fwrite(block_data, 1, channels, tga_file);
		}
	}

	// write footer (optional, but the specification recommends it)
	memset(footer, 0, sizeof(footer));
	strncpy(&footer[8] , "TRUEVISION-XFILE", 16);
	footer[24] = '.';
	footer[25] = 0;
	Img_fwrite(footer, 1, sizeof(footer), tga_file);

	fclose(tga_file);
}
