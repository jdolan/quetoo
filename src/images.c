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

#include "images.h"

#ifdef BUILD_CLIENT

/* Work-around for a conflict between windows.h and jpeglib.h.
 If ADDRESS_TAG_BIT is defined then BaseTsd.h has been included and
 INT32 has been defined with a typedef, so we must define XMD_H to
 prevent the jpeg header from defining it again. */

/* And another one... jmorecfg.h defines the 'boolean' type as int,
 which conflicts with the standard Windows 'boolean' definition as
 byte. Ref: http://www.asmail.be/msg0054688232.html */

#if defined(_WIN32)
/* typedef "boolean" as byte to match rpcndr.h */
typedef byte boolean;
#define HAVE_BOOLEAN    /* prevent jmorecfg.h from typedef-ing it as int32_t */
#endif

# if defined(__WIN32__) && defined(ADDRESS_TAG_BIT) && !defined(XMD_H)
#  define XMD_H
#  define VTK_JPEG_XMD_H
# endif
# include <jpeglib.h>
# if defined(VTK_JPEG_XMD_H)
#  undef VTK_JPEG_XMD_H
#  undef XMD_H
# endif

// 8bit palette for wal images and particles
#define PALETTE "pics/colormap"
uint32_t palette[256];
bool palette_initialized = false;

// .wal file header for loading legacy .wal textures
typedef struct miptex_s {
	char name[32];
	uint32_t width, height;
	uint32_t offsets[4]; // four mip maps stored
	char animname[32]; // next frame in animation chain
	uint32_t flags;
	int32_t contents;
	int32_t value;
} miptex_t;

// default pixel format to which all images are converted
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000

SDL_PixelFormat format = { NULL, // palette
		32, // bits
		4, // bytes
		0, // rloss
		0, // gloss
		0, // bloss
		0, // aloss
		0, // rshift
		8, // gshift
		16, // bshift
		24, // ashift
		RMASK, // rmask
		GMASK, // gmask
		BMASK, // bmask
		AMASK, // amask
		0, // colorkey
		1 // alpha
		};

// image formats, tried in this order
const char *IMAGE_TYPES[] = { "tga", "png", "jpg", "wal", "pcx", NULL };

/*
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface. Image formats are tried in the order they appear
 * in TYPES.
 */
bool Img_LoadImage(const char *name, SDL_Surface **surf) {
	int32_t i;

	i = 0;
	while (IMAGE_TYPES[i]) {
		if (Img_LoadTypedImage(name, IMAGE_TYPES[i++], surf))
			return true;
	}

	return false;
}

/*
 * @brief A helper which mangles a .wal file into an SDL_Surface suitable for
 * OpenGL uploads and other basic manipulations.
 */
static bool Img_LoadWal(const char *path, SDL_Surface **surf) {
	void *buf;
	uint32_t i;
	byte *b;

	*surf = NULL;

	if (Fs_Load(path, &buf) == -1)
		return false;

	miptex_t *mt = (miptex_t *) buf;

	mt->width = LittleLong(mt->width);
	mt->height = LittleLong(mt->height);

	mt->offsets[0] = LittleLong(mt->offsets[0]);

	if (!palette_initialized) // lazy-load palette if necessary
		Img_InitPalette();

	size_t size = mt->width * mt->height;
	uint32_t *p = (uint32_t *) malloc(size * sizeof(uint32_t));

	b = (byte *) mt + mt->offsets[0];

	for (i = 0; i < size; i++) { // convert to 32bpp RGBA via palette
		if (b[i] == 255) // transparent
			p[i] = 0;
		else
			p[i] = palette[b[i]];
	}

	Fs_Free(buf);

	// create the RGBA surface
	if ((*surf = SDL_CreateRGBSurfaceFrom(p, mt->width, mt->height, 32, 0, RMASK, GMASK, BMASK,
			AMASK))) {

		// trick SDL into freeing the pixel data with the surface
		(*surf)->flags &= ~SDL_PREALLOC;
	}

	return *surf != NULL;
}

/*
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface.
 */
bool Img_LoadTypedImage(const char *name, const char *type, SDL_Surface **surf) {
	char path[MAX_QPATH];
	void *buf;
	int64_t len;

	g_snprintf(path, sizeof(path), "%s.%s", name, type);

	if (!strcmp(type, "wal")) { // special case for .wal files
		return Img_LoadWal(path, surf);
	}

	*surf = NULL;

	if ((len = Fs_Load(path, &buf)) != -1) {

		SDL_RWops *rw;
		if ((rw = SDL_RWFromMem(buf, len))) {

			SDL_Surface *s;
			if ((s = IMG_LoadTyped_RW(rw, 0, (char *) type))) {

				if (!g_str_has_prefix(path, PALETTE)) {
					*surf = SDL_ConvertSurface(s, &format, 0);
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

/*
 * @brief Initializes the 8bit color palette required for .wal texture loading.
 */
void Img_InitPalette(void) {
	SDL_Surface *surf;
	byte r, g, b;
	uint32_t v;
	int32_t i;

	if (!Img_LoadTypedImage(PALETTE, "pcx", &surf))
		return;

	for (i = 0; i < 256; i++) {
		r = surf->format->palette->colors[i].r;
		g = surf->format->palette->colors[i].g;
		b = surf->format->palette->colors[i].b;

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		palette[i] = LittleLong(v);
	}

	palette[255] &= LittleLong(0xffffff); // 255 is transparent

	SDL_FreeSurface(surf);

	palette_initialized = true;
}

/*
 * @brief Returns RGB components of the specified color in the specified result array.
 */
void Img_ColorFromPalette(byte c, float *res) {
	uint32_t color;

	if (!palette_initialized) // lazy-load palette if necessary
		Img_InitPalette();

	color = palette[c];

	res[0] = (color >> 0 & 255) / 255.0;
	res[1] = (color >> 8 & 255) / 255.0;
	res[2] = (color >> 16 & 255) / 255.0;
}

/*
 * @brief Write pixel data to a JPEG file.
 */
bool Img_WriteJPEG(const char *path, byte *data, uint32_t width, uint32_t height, int32_t quality) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
	FILE *f;

	if (!(f = fopen(va("%s/%s", Fs_WriteDir(), path), "wb"))) {
		Com_Print("Failed to open to %s\n", path);
		return false;
	}

	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_compress(&cinfo);

	jpeg_stdio_dest(&cinfo, f);

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);

	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	const uint32_t stride = width * 3; // bytes per scanline

	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &data[(cinfo.image_height - cinfo.next_scanline - 1) * stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);

	jpeg_destroy_compress(&cinfo);

	fclose(f);
	return true;
}

#endif /* BUILD_CLIENT */
