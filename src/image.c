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

#include "image.h"

#define IMG_PALETTE "pics/colormap.pcx"

img_palette_t img_palette;
static _Bool img_palette_initialized;

// image formats, tried in this order
static const char *img_formats[] = { "tga", "png", "jpg", "pcx", NULL };

/**
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface.
 */
static _Bool Img_LoadTypedImage(const char *name, const char *type, SDL_Surface **surf) {
	char path[MAX_QPATH];
	void *buf;
	int64_t len;

	g_snprintf(path, sizeof(path), "%s.%s", name, type);

	*surf = NULL;

	if ((len = Fs_Load(path, &buf)) != -1) {

		SDL_RWops *rw;
		if ((rw = SDL_RWFromConstMem(buf, (int32_t) len))) {

			SDL_Surface *s;
			if ((s = IMG_LoadTyped_RW(rw, 0, (char *) type))) {

				if (g_strcmp0(path, IMG_PALETTE) && s->format->format != SDL_PIXELFORMAT_RGBA32) {
					*surf = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);
					SDL_FreeSurface(s);
				} else {
					*surf = s;
				}
			}
			SDL_RWclose(rw);
		}
		Fs_Free(buf);
	}

	return *surf != NULL;
}

/**
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface. Image formats are tried in the order they appear
 * in TYPES.
 */
_Bool Img_LoadImage(const char *name, SDL_Surface **surf) {

	char basename[MAX_QPATH];
	StripExtension(name, basename);

	int32_t i = 0;
	while (img_formats[i]) {
		if (Img_LoadTypedImage(basename, img_formats[i++], surf)) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Initializes the 8bit color palette required for .wal texture loading.
 */
void Img_InitPalette(void) {
	SDL_Surface *surf;

	if (!Img_LoadTypedImage(IMG_PALETTE, "pcx", &surf)) {
		return;
	}

	for (size_t i = 0; i < lengthof(img_palette); i++) {
		const byte r = surf->format->palette->colors[i].r;
		const byte g = surf->format->palette->colors[i].g;
		const byte b = surf->format->palette->colors[i].b;

		const uint32_t v = (255u << 24) + (r << 0) + (g << 8) + (b << 16);
		img_palette[i] = LittleLong(v);
	}

	img_palette[lengthof(img_palette) - 1] &= LittleLong(0xffffff); // 255 is transparent

	SDL_FreeSurface(surf);

	img_palette_initialized = true;
}

/**
 * @brief Returns RGB components of the specified color in the specified result array.
 */
color_t Img_ColorFromPalette(uint8_t c) {

	if (!img_palette_initialized) {
		Img_InitPalette();
	}

	return Color4bv(img_palette[c]);
}

/**
* @brief Write pixel data to a PNG file.
*/
_Bool Img_WritePNG(const char *path, byte *data, uint32_t width, uint32_t height) {
	SDL_RWops *f;
	const char *real_path = Fs_RealPath(path);

	if (!(f = SDL_RWFromFile(real_path, "wb"))) {
		Com_Warn("Failed to open to %s\n", real_path);
		return false;
	}

	byte *buffer = Mem_Malloc(width * height * 3);

	// Flip pixels vertically
	for (size_t i = 0; i < height; i++) {
		memcpy(buffer + (height - i - 1) * width * 3, data + i * width * 3, 3 * width);
	}

	SDL_Surface *ss = SDL_CreateRGBSurfaceFrom(buffer, width, height, 8 * 3, width * 3, 0, 0, 0, 0);
	IMG_SavePNG_RW(ss, f, 0);

	SDL_FreeSurface(ss);
	Mem_Free(buffer);
	SDL_RWclose(f);
	return true;
}

// there are _0 and _1 in here just to prevent padding
// and so I can control the bytes explicitly. Dumb C.
typedef struct {
	uint8_t IDLength;        /* 00h  Size of Image ID field */
	uint8_t ColorMapType;    /* 01h  Color map type */
	uint8_t ImageType;       /* 02h  Image type code */
	uint8_t CMapStart_0, CMapStart_1;      /* 03h  Color map origin */
	uint8_t CMapLength_0, CMapLength_1;     /* 05h  Color map length */
	uint8_t CMapDepth;       /* 07h  Depth of color map entries */
	uint8_t XOffset_0, XOffset_1;        /* 08h  X origin of image */
	uint8_t YOffset_0, YOffset_1;        /* 0Ah  Y origin of image */
	uint16_t Width;          /* 0Ch  Width of image */
	uint16_t Height;         /* 0Eh  Height of image */
	uint8_t PixelDepth;      /* 10h  Image pixel size */
	uint8_t ImageDescriptor; /* 11h  Image descriptor byte */
} r_tga_header_t;

/**
* @brief Write pixel data to a TGA file.
*/
_Bool Img_WriteTGA(const char *path, byte *data, uint32_t width, uint32_t height) {
	SDL_RWops *f;
	const char *real_path = Fs_RealPath(path);

	if (!(f = SDL_RWFromFile(real_path, "wb"))) {
		Com_Warn("Failed to open to %s\n", real_path);
		return false;
	}

	const r_tga_header_t header = {
		0, // no image data
		0, // no colormap
		2, // truecolor, no colormap, no encoding
		0, 0, // colormap
		0, 0, // colormap
		0, // colormap
		0, 0, // X origin
		0, 0, // Y origin
		width, // width
		height, // height
		24, // depth
		0 //descriptor
	};

	// write TGA header
	SDL_RWwrite(f, &header, 18, 1);

	// write TGA data
	SDL_RWwrite(f, data, width * height, 3);

	SDL_RWclose(f);
	return true;
}

/**
* @brief Write pixel data to a PBM file.
*/
_Bool Img_WritePBM(const char *path, byte *data, uint32_t width, uint32_t height, uint32_t bpp) {
	SDL_RWops *f;
	const char *real_path = Fs_RealPath(path);

	if (!(f = SDL_RWFromFile(real_path, "wb"))) {
		Com_Warn("Failed to open to %s\n", real_path);
		return false;
	}

	char header[256];

	if (bpp == 4) {
		g_snprintf(header, sizeof(header), "PF\n%u %u\n%f\n", width, height, -1.0f);
	}
	else {
		g_snprintf(header, sizeof(header), "P6\n%u %u\n%d\n", width, height, bpp == 2 ? 65535 : 255);
	}

	// write PBM header
	SDL_RWwrite(f, header, strlen(header), 1);

	// output buffer
	byte *buffer = Mem_Malloc(width * height * 3 * bpp);
	memcpy(buffer, data, width * height * 3 * bpp);

	// possible input/output buffers in needed formats
	const uint8_t *buffer_uint8_in = data;
	uint8_t *buffer_uint8_out = buffer;

	const uint16_t *buffer_uint16_in = (uint16_t *)data;
	uint16_t *buffer_uint16_out = (uint16_t *)buffer;

	const float *buffer_float_in = (float *)data;
	float *buffer_float_out = (float *)buffer;

	uint8_t *chunk = NULL;
	
	// swap to big endian and flip pixels vertically (if needed)
	for (size_t i = 0; i < height; i++) {
		for (size_t j = 0; j < width * 3; j++) {
			size_t index_in = i * width * 3 + j;
			size_t index_out = (height - i - 1) * width * 3 + j;

			switch (bpp) {
			case 1:
				buffer_uint8_out[index_out] = buffer_uint8_in[index_in];
				break;
			case 2:
				chunk = (uint8_t *)(&buffer_uint16_in[index_in]);
				buffer_uint16_out[index_out] = (chunk[1] << 0) | (chunk[0] << 8);
				break;
			case 4:
				buffer_float_out[index_out] = buffer_float_in[index_in];
				break;
			}
		}
	}

	SDL_RWwrite(f, buffer, width * height, 3 * bpp);

	Mem_Free(buffer);

	SDL_RWclose(f);
	return true;
}
