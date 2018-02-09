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

#define IMG_PALETTE "pics/colormap"

img_palette_t img_palette;
static _Bool img_palette_initialized;

// RGBA color masks
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000

// image formats, tried in this order
static const char *img_formats[] = { "tga", "png", "jpg", "wal", "pcx", NULL };

/**
 * @brief A helper which mangles a .wal file into an SDL_Surface suitable for
 * OpenGL uploads and other basic manipulations.
 */
static _Bool Img_LoadWal(const char *path, SDL_Surface **surf) {
	void *buf;

	*surf = NULL;

	if (Fs_Load(path, &buf) == -1) {
		return false;
	}

	d_wal_t *wal = (d_wal_t *) buf;

	wal->width = LittleLong(wal->width);
	wal->height = LittleLong(wal->height);

	wal->offsets[0] = LittleLong(wal->offsets[0]);

	if (!img_palette_initialized) { // lazy-load palette if necessary
		Img_InitPalette();
	}

	size_t size = wal->width * wal->height;
	uint32_t *p = (uint32_t *) SDL_malloc(size * sizeof(uint32_t));

	const byte *b = (byte *) wal + wal->offsets[0];
	for (size_t i = 0; i < size; i++) { // convert to 32bpp RGBA via palette
		if (b[i] == 255) { // transparent
			p[i] = 0;
		} else {
			p[i] = img_palette[b[i]];
		}
	}

	// create the RGBA surface
	if ((*surf = SDL_CreateRGBSurfaceFrom(p, wal->width, wal->height, 32, 0,
	                                      RMASK, GMASK, BMASK, AMASK))) {

		// trick SDL into freeing the pixel data with the surface
		(*surf)->flags &= ~SDL_PREALLOC;
	}

	Fs_Free(buf);

	return *surf != NULL;
}

/**
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface.
 */
static _Bool Img_LoadTypedImage(const char *name, const char *type, SDL_Surface **surf) {
	char path[MAX_QPATH];
	void *buf;
	int64_t len;

	g_snprintf(path, sizeof(path), "%s.%s", name, type);

	if (!g_strcmp0(type, "wal")) { // special case for .wal files
		return Img_LoadWal(path, surf);
	}

	*surf = NULL;

	if ((len = Fs_Load(path, &buf)) != -1) {

		SDL_RWops *rw;
		if ((rw = SDL_RWFromMem(buf, (int32_t) len))) {

			SDL_Surface *s;
			if ((s = IMG_LoadTyped_RW(rw, 0, (char *) type))) {

				if (!g_str_has_prefix(path, IMG_PALETTE) && s->format->format != SDL_PIXELFORMAT_ABGR8888) {
					*surf = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
					SDL_FreeSurface(s);
				} else {
					*surf = s;
				}
			}
			SDL_FreeRW(rw);
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
void Img_ColorFromPalette(uint8_t c, vec_t *res) {

	if (!img_palette_initialized) { // lazy-load palette if necessary
		Img_InitPalette();
	}

	const uint32_t color = img_palette[c];

	res[0] = (color >> 0 & 255) / 255.0;
	res[1] = (color >> 8 & 255) / 255.0;
	res[2] = (color >> 16 & 255) / 255.0;
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
