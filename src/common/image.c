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

#include <complex.h>
#include <fftw3.h>

/**
 * @brief Loads the specified image from the game filesystem.
 */
static SDL_Surface *Img_LoadSurface_(const char *name, const char *type) {
	SDL_Surface *surf = NULL;

	char path[MAX_QPATH];
	g_snprintf(path, sizeof(path), "%s.%s", name, type);

	void *buf;
	int64_t len;
	if ((len = Fs_Load(path, &buf)) != -1) {

		SDL_RWops *rw;
		if ((rw = SDL_RWFromConstMem(buf, (int32_t) len))) {

			SDL_Surface *s;
			if ((s = IMG_LoadTyped_RW(rw, 0, type))) {

				if (s->format->format != SDL_PIXELFORMAT_RGBA32) {
					surf = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);
					SDL_FreeSurface(s);
				} else {
					surf = s;
				}
			}
			SDL_RWclose(rw);
		}
		Fs_Free(buf);
	}

	return surf;
}

/**
 * @brief Loads the specified image from the game filesystem, trying all supported formats.
 */
SDL_Surface *Img_LoadSurface(const char *name) {
	const char *img_formats[] = { "tga", "png", "jpg", NULL };

	char basename[MAX_QPATH];
	StripExtension(name, basename);

	for (const char **fmt = img_formats; *fmt; fmt++) {
		SDL_Surface *surf = Img_LoadSurface_(basename, *fmt);
		if (surf) {
			return surf;
		}
	}

	return NULL;
}

/**
 * @brief Resolves the average color of the specified surface.
 */
color_t Img_Color(const SDL_Surface *surf) {
	color_t out = color_white;

	if (surf) {
		const int32_t texels = surf->w * surf->h;
		uint64_t r = 0, g = 0, b = 0;

		for (int32_t j = 0; j < texels; j++) {

			const byte *pos = (byte *) surf->pixels + j * 4;

			r += *pos++;
			g += *pos++;
			b += *pos++;
		}

		out = Color3f((r / (float) texels) / 255.f,
					  (g / (float) texels) / 255.f,
					  (b / (float) texels) / 255.f);
	}

	return out;
}

// Quick access of a pixel
#define SDL_PIXEL_AT(surf, type, x, y) \
	*(type *)((byte *)surf->pixels + ((y) * surf->pitch) + (x) * surf->format->BytesPerPixel)

// helper macro which copies an SDL pixel from one surf to another
#define SDL_COPY_PIXEL(from, to, type, src_x, src_y, dst_x, dst_y) \
		SDL_PIXEL_AT(to, type, dst_x, dst_y) = SDL_PIXEL_AT(from, type, src_x, src_y)

/**
 * @brief Rotate an SDL surface counter-clockwise by the number of rotations specified.
 * @param surf Surface to rotate. It is not modified.
 * @param num_rotations Number of 90-degree rotations to rotate by.
 * @return Either a reference to "surf" if the surface was not rotated, or a new surface.
*/
SDL_Surface *Img_RotateSurface(SDL_Surface *surf, int32_t num_rotations) {

	num_rotations %= 4;

	if (!num_rotations) {
		return surf;
	}

	if (surf->w != surf->h) {
		Com_Error(ERROR_FATAL, "Only works on square images :(");
	}

	SDL_LockSurface(surf);

	SDL_Surface *output = SDL_CreateRGBSurfaceWithFormat(0, surf->w, surf->h, surf->format->BitsPerPixel, surf->format->format);

	SDL_LockSurface(output);

	switch (num_rotations) {
	case 1:
		for (int32_t y = 0; y < surf->h; y++)
			for (int32_t x = 0; x < surf->w; x++)
				SDL_COPY_PIXEL(surf, output, int32_t, surf->w - y - 1, x, x, y);
		break;
	case 2:
		for (int32_t y = 0; y < surf->h; y++)
			for (int32_t x = 0; x < surf->w; x++)
				SDL_COPY_PIXEL(surf, output, int32_t, surf->w - x - 1, surf->h - y - 1, x, y);
		break;
	case 3:
		for (int32_t y = 0; y < surf->h; y++)
			for (int32_t x = 0; x < surf->w; x++)
				SDL_COPY_PIXEL(surf, output, int32_t, y, surf->h - x - 1, x, y);
		break;
	}

	SDL_UnlockSurface(output);

	SDL_UnlockSurface(surf);

	return output;
}

/**
 * @brief Compute the brightness (first derivative) from the given diffusemap.
 *
 * @param dx The horizontal gradient output.
 * @param dy The vertical gradient output.
 */
static void Img_CreateHeightmap_diffusemap_derivative(const SDL_Surface* diffusemap, float *dx, float *dy) {

	const int32_t w = diffusemap->w;
	const int32_t h = diffusemap->h;

	const color32_t *in = (color32_t *) diffusemap->pixels;
	for (int32_t y = 1; y < h - 1; y++) {
		for (int32_t x = 1; x < w - 1; x++, in++, dx++, dy++) {

			const float brightness = Vec3_Length(Color32_Color(*in).vec3);
			*dx = brightness;
			*dy = brightness;
		}
	}
}

/**
 * @brief Compute the gradients (first derivative) from the given normalmap.
 *
 * @param dx The horizontal gradient output.
 * @param dy The vertical gradient output.
 */
static void Img_CreateHeightmap_normalmap_derivative(const SDL_Surface* normalmap, float *dx, float *dy) {

	const int32_t w = normalmap->w;
	const int32_t h = normalmap->h;

	const color32_t *in = (color32_t *) normalmap->pixels;
	for (int32_t y = 1; y < h - 1; y++) {
		for (int32_t x = 1; x < w - 1; x++, in++, dx++, dy++) {

			const vec3_t normal = Color32_Direction(*in);
			if (normal.z != 0.f) {
				*dx = -normal.x / normal.z;
				*dy = -normal.y / normal.z;
			} else {
				*dx = 0.f;
				*dy = 0.f;
			}
		}
	}
}

/**
 * @brief
 */
void Img_CreateHeightmap3(const SDL_Surface *diffusemap, SDL_Surface* normalmap, SDL_Surface *heightmap) {

	const int32_t w = normalmap->w;
	const int32_t h = normalmap->h;

	float *dx = (float *) calloc(w * h, sizeof(float));
	float *dy = (float *) calloc(w * h, sizeof(float));
	float *dh = (float *) calloc(w * h, sizeof(float));

	Img_CreateHeightmap_normalmap_derivative(normalmap, dx, dy);

	// Integrate the X direction
	for (int32_t y = 0; y < h; y++) {
		for (int32_t x = 1; x < w; x++) {
			dh[y * w + x] += dh[y * w + (x - 1)] + dx[y * w + x];
		}
	}

	// Integrate the Y direction
	for (int32_t x = 0; x < w; x++) {
		for (int32_t y = 1; y < h; y++) {
			dh[y * w + x] += dh[(y - 1) * w + x] + dy[y * w + x];
		}
	}

	free(dx);
	free(dy);

	// Find the height range
	float min = dh[0], max = dh[0];
	for (int32_t i = 1; i < w * h; i++) {
		min = Minf(min, dh[i]);
		max = Maxf(max, dh[i]);
	}

	// Normalize the height values to 0-255 and store in normalmap alpha channel
	color32_t *out_normalmap = normalmap->pixels;
	color32_t *out_heightmap = heightmap ? heightmap->pixels : NULL;

	for (int32_t y = 0; y < h; y++) {
		for (int32_t x = 0; x < w; x++) {

			const float height = (dh[y * w + x] - min) / (max - min);
			const byte alpha = (byte) (height * 255);

			out_normalmap->a = alpha;
			out_normalmap++;

			if (out_heightmap) {
				*out_heightmap++ = Color32(alpha, alpha, alpha, 255);
			}
		}
	}

	free(dh);
}

/**
 * @brief
 */
void Img_CreateHeightmap2(const SDL_Surface *diffusemap, SDL_Surface* normalmap, SDL_Surface *heightmap) {

	const int32_t w = normalmap->w;
	const int32_t h = normalmap->h;

	float *dx = fftwf_alloc_real(w * h);
	float *dy = fftwf_alloc_real(w * h);

	Img_CreateHeightmap_diffusemap_derivative(diffusemap, dx, dy);

	float *nx = fftwf_alloc_real(w * h);
	float *ny = fftwf_alloc_real(w * h);

	Img_CreateHeightmap_normalmap_derivative(normalmap, nx, ny);

	const int32_t half_width = w / 2 + 1;
	fftwf_complex *Nx = fftwf_alloc_complex(half_width * h);
	fftwf_complex *Ny = fftwf_alloc_complex(half_width * h);
	fftwf_complex *Dx = fftwf_alloc_complex(half_width * h);
	fftwf_complex *Dy = fftwf_alloc_complex(half_width * h);

	fftwf_plan plan = fftwf_plan_dft_r2c_2d(h, w, nx, Nx, FFTW_ESTIMATE);
	fftwf_execute_dft_r2c(plan, nx, Nx);
	fftwf_execute_dft_r2c(plan, ny, Ny);
	fftwf_execute_dft_r2c(plan, dx, Dx);
	fftwf_execute_dft_r2c(plan, dy, Dy);
	fftwf_destroy_plan(plan);

	float *r = fftwf_alloc_real(w * h);
	fftwf_complex *R = fftwf_alloc_complex(half_width * h);

	int32_t i = 0;
	for (int32_t y = 0; y < h; y++) {
		for (int32_t x = 0; x < half_width; x++, i++) {
			const float denom = w * h * (Dx[i] * Dx[i] + Dy[i] * Dy[i]);
			R[i] = denom > 0.f ? -(Dx[i] * Nx[i] + Dy[i] * Ny[i]) / denom : 0.f;
		}
	}

	plan = fftwf_plan_dft_c2r_2d(h, w, R, r, FFTW_ESTIMATE);
	fftwf_execute(plan);
	fftwf_destroy_plan(plan);

	fftwf_free(nx);
	fftwf_free(ny);
	fftwf_free(dx);
	fftwf_free(dy);

	fftwf_free(Nx);
	fftwf_free(Ny);
	fftwf_free(Dx);
	fftwf_free(Dy);

	// Normalize the height values to 0-255 and store in normalmap alpha channel
	float min = r[0], max = r[0];
	for (int32_t i = 1; i < w * h; i++) {
		min = Minf(min, r[i]);
		max = Maxf(max, r[i]);
	}

	color32_t *out_normalmap = normalmap->pixels;
	color32_t *out_heightmap = heightmap ? heightmap->pixels : NULL;

	for (int32_t y = 0; y < h; y++) {
		for (int32_t x = 0; x < w; x++) {

			const float height = (r[y * w + x] - min) / (max - min);
			const byte alpha = (byte) (height * 255);

			out_normalmap->a = alpha;
			out_normalmap++;

			if (out_heightmap) {
				*out_heightmap++ = Color32(alpha, alpha, alpha, 255);
			}
		}
	}

	fftwf_free(r);
	fftwf_free(R);
}

/**
 * @brief
 */
void Img_CreateHeightmap(const SDL_Surface *diffusemap, SDL_Surface* normalmap, SDL_Surface *heightmap) {

	const int32_t w = normalmap->w;
	const int32_t h = normalmap->h;

	const color32_t *in_diffusemap = diffusemap->pixels;
	color32_t *in_normalmap = normalmap->pixels;

	color32_t *out_heightmap = heightmap ? heightmap->pixels : NULL;

	int32_t i = 0;
	for (int32_t y = 0; y < h; y++) {
		for (int32_t x = 1; x < w; x++, i++, in_diffusemap++, in_normalmap++) {

			const vec3_t diffuse = Color32_Color(*in_diffusemap).vec3;
			const vec3_t normal = Color32_Direction(*in_normalmap);

			const byte height = Clampf(Vec3_Length(diffuse) * normal.z, 0.f, 1.f) * 255;

			in_normalmap->a = height;
			if (out_heightmap) {
				*out_heightmap++ = Color32(height, height, height, 255);
			}
		}
	}
}

/**
* @brief Write pixel data to a PNG file.
*/
bool Img_WritePNG(const char *path, byte *data, uint32_t width, uint32_t height) {
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
bool Img_WriteTGA(const char *path, byte *data, uint32_t width, uint32_t height) {
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
bool Img_WritePBM(const char *path, byte *data, uint32_t width, uint32_t height, uint32_t bpp) {
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

	const uint8_t *chunk = NULL;
	
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
				chunk = (const uint8_t *)(&buffer_uint16_in[index_in]);
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
