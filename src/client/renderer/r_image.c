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

#include "r_local.h"

r_image_t *r_null_image; // use for bad textures
r_image_t *r_particle_image; // little dot for particles
r_image_t *r_explosion_image; // expanding explosion particle
r_image_t *r_teleport_image; // teleport ring particle
r_image_t *r_smoke_image; // smoke for rocket/grenade trails
r_image_t *r_steam_image; // smoke for rocket/grenade trails
r_image_t *r_bubble_image; // bubble trails under water
r_image_t *r_rain_image; // atmospheric rain
r_image_t *r_snow_image; // and snow effects
r_image_t *r_beam_image; // rail trail beams
r_image_t *r_burn_image; // burn marks from hyperblaster
r_image_t *r_blood_image; // blood mist
r_image_t *r_lightning_image; // lightning particles
r_image_t *r_rail_trail_image; // rail spiral
r_image_t *r_flame_image; // flames
r_image_t *r_spark_image; // sparks
r_image_t *r_bullet_images[NUM_BULLET_IMAGES]; // bullets hitting walls

r_image_t *r_envmap_images[NUM_ENVMAP_IMAGES]; // generic environment map

r_image_t *r_flare_images[NUM_FLARE_IMAGES]; // lense flares

r_image_t *r_warp_image; // fragment program warping

r_image_t r_images[MAX_GL_TEXTURES];
unsigned short r_num_images;

GLint r_filter_min = GL_LINEAR_MIPMAP_NEAREST;
GLint r_filter_max = GL_LINEAR;
GLfloat r_filter_aniso = 1.0;

#define MAX_TEXTURE_SIZE 2048

typedef struct {
	const char *name;
	GLenum minimize, maximize;
} r_texture_mode_t;

r_texture_mode_t r_texture_modes[] = {
		{ "GL_NEAREST", GL_NEAREST, GL_NEAREST }, { "GL_LINEAR", GL_LINEAR,
				GL_LINEAR }, { "GL_NEAREST_MIPMAP_NEAREST",
				GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
		{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR }, {
				"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR,
				GL_NEAREST }, { "GL_LINEAR_MIPMAP_LINEAR",
				GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR } };

#define NUM_GL_TEXTURE_MODES (sizeof(r_texture_modes) / sizeof(r_texture_mode_t))

/*
 * R_texture_mode
 */
void R_TextureMode(const char *mode) {
	r_image_t *image;
	unsigned short i;

	for (i = 0; i < NUM_GL_TEXTURE_MODES; i++) {
		if (!strcasecmp(r_texture_modes[i].name, mode))
			break;
	}

	if (i == NUM_GL_TEXTURE_MODES) {
		Com_Warn("R_texture_mode: Bad filter name.\n");
		return;
	}

	r_filter_min = r_texture_modes[i].minimize;
	r_filter_max = r_texture_modes[i].maximize;

	if (r_anisotropy->value)
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_filter_aniso);
	else
		r_filter_aniso = 1.0;

	// change all the existing mipmap texture objects
	for (i = 0, image = r_images; i < r_num_images; i++, image++) {

		if (!image->texnum)
			continue;

		if (image->type == it_font || image->type == it_pic || image->type
				== it_sky)
			continue; // no mipmaps

		R_BindTexture(image->texnum);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_max);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
				r_filter_aniso);
	}
}

/*
 * R_ListImages_f
 */
void R_ListImages_f(void) {
	int i;
	r_image_t *image;
	int texels;

	texels = 0;
	for (i = 0, image = r_images; i < r_num_images; i++, image++) {

		if (!image->texnum)
			continue;

		texels += image->upload_width * image->upload_height;

		switch (image->type) {
		case it_font:
			Com_Print("C");
			break;
		case it_effect:
			Com_Print("E");
			break;
		case it_world:
			Com_Print("W");
			break;
		case it_normalmap:
			Com_Print("N");
			break;
		case it_material:
			Com_Print("M");
			break;
		case it_sky:
			Com_Print("K");
			break;
		case it_skin:
			Com_Print("S");
			break;
		case it_pic:
			Com_Print("P");
			break;
		default:
			Com_Print(" ");
			break;
		}

		Com_Print(" %4ix%4i: %s\n", image->upload_width, image->upload_height,
				image->name);
	}
	Com_Print("Total texel count (not counting mipmaps or lightmaps): %i\n",
			texels);
}

#define MAX_SCREENSHOTS 100

/*
 * R_Screenshot_f
 */
void R_Screenshot_f(void) {
	static int last_shot; // small optimization, don't fopen so many times
	char file_name[MAX_OSPATH];
	int i, quality;
	byte *buffer;
	FILE *f;

	void (*Img_Write)(const char *path, byte *img_data, int width, int height,
			int quality);

	// use format specified in type cvar
	if (!strcmp(r_screenshot_type->string, "jpeg") || !strcmp(
			r_screenshot_type->string, "jpg")) {
		Img_Write = &Img_WriteJPEG;
	} else if (!strcmp(r_screenshot_type->string, "tga")) {
		Img_Write = &Img_WriteTGARLE;
	} else {
		Com_Warn("R_Screenshot_f: Type \"%s\" not supported.\n",
				r_screenshot_type->string);
		return;
	}

	// create the screenshots directory if it doesn't exist
	snprintf(file_name, sizeof(file_name), "%s/screenshots/", Fs_Gamedir());
	Fs_CreatePath(file_name);

	// find a file name to save it to
	for (i = last_shot; i < MAX_SCREENSHOTS; i++) {

		snprintf(file_name, sizeof(file_name), "%s/screenshots/quake2world%02d.%s",
				Fs_Gamedir(), i, r_screenshot_type->string);

		if (!(f = fopen(file_name, "rb")))
			break; // file doesn't exist

		Fs_CloseFile(f);
	}

	if (i == MAX_SCREENSHOTS) {
		Com_Warn("R_Screenshot_f: Failed to create %s\n", file_name);
		return;
	}

	last_shot = i;

	buffer = Z_Malloc(r_context.width * r_context.height * 3);

	glReadPixels(0, 0, r_context.width, r_context.height, GL_RGB,
			GL_UNSIGNED_BYTE, buffer);

	quality = r_screenshot_quality->value * 100;

	if (quality < 0) // clamp it
		quality = 0;

	if (quality > 100)
		quality = 100;

	(*Img_Write)(file_name, buffer, r_context.width, r_context.height, quality);

	Z_Free(buffer);
	Com_Print("Saved %s\n", Basename(file_name));
}

/*
 * R_SoftenTexture
 */
void R_SoftenTexture(byte *in, int width, int height, r_image_type_t type) {
	byte *out;
	int i, j, k, bpp;
	byte *src, *dest;
	byte *u, *d, *l, *r;

	if (type == it_lightmap || type == it_deluxemap)
		bpp = 3;
	else
		bpp = 4;

	// soften into a copy of the original image, as in-place would be incorrect
	out = (byte *) Z_Malloc(width * height * bpp);
	memcpy(out, in, width * height * bpp);

	for (i = 1; i < height - 1; i++) {
		for (j = 1; j < width - 1; j++) {

			src = in + ((i * width) + j) * bpp; // current input pixel

			u = (src - (width * bpp)); // and it's neighbors
			d = (src + (width * bpp));
			l = (src - (1 * bpp));
			r = (src + (1 * bpp));

			dest = out + ((i * width) + j) * bpp; // current output pixel

			for (k = 0; k < bpp; k++)
				dest[k] = (u[k] + d[k] + l[k] + r[k]) / 4;
		}
	}

	// copy the softened image over the input image, and free it
	memcpy(in, out, width * height * bpp);
	Z_Free(out);
}

/*
 * R_ScaleTexture
 */
static void R_ScaleTexture(const unsigned *in, int inwidth, int inheight,
		unsigned *out, int outwidth, int outheight) {
	int i, j;
	unsigned frac, fracstep;
	unsigned p1[MAX_TEXTURE_SIZE], p2[MAX_TEXTURE_SIZE];

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++) {
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++) {
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth) {
		const unsigned *inrow = in + inwidth * (int) ((i + 0.25) * inheight
				/ outheight);
		const unsigned *inrow2 = in + inwidth * (int) ((i + 0.75) * inheight
				/ outheight);
		for (j = 0; j < outwidth; j++) {
			const byte *pix1 = (const byte *) inrow + p1[j];
			const byte *pix2 = (const byte *) inrow + p2[j];
			const byte *pix3 = (const byte *) inrow2 + p1[j];
			const byte *pix4 = (const byte *) inrow2 + p2[j];
			((byte *) (out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])
					>> 2;
			((byte *) (out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])
					>> 2;
			((byte *) (out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])
					>> 2;
			((byte *) (out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])
					>> 2;
		}
	}
}

/*
 * R_FilterTexture
 *
 * Applies brightness and contrast to the specified image while optionally computing
 * the image's average color.  Also handles image inversion and monochrome.  This is
 * all munged into one function to reduce loops on level load.
 */
void R_FilterTexture(byte *in, int width, int height, vec3_t color,
		r_image_type_t type) {
	vec3_t temp;
	int i, j, c, bpp, mask;
	unsigned col[3];
	byte *p;
	float brightness;

	p = in;
	c = width * height;

	bpp = mask = 0; // monochrome / invert
	brightness = r_brightness->value;

	if (type == it_world || type == it_effect || type == it_material) {
		bpp = 4;
		mask = 1;
	} else if (type == it_lightmap) {
		bpp = 3;
		mask = 2;

		brightness = r_modulate->value;
	}

	if (color) // compute average color
		VectorClear(col);

	for (i = 0; i < c; i++, p += bpp) {

		VectorScale(p, 1.0 / 255.0, temp); // convert to float

		// apply brightness, saturation and contrast
		ColorFilter(temp, temp, brightness, r_saturation->value,
				r_contrast->value);

		for (j = 0; j < 3; j++) {

			temp[j] *= 255; // back to byte

			if (temp[j] > 255) // clamp
				temp[j] = 255;
			else if (temp[j] < 0)
				temp[j] = 0;

			p[j] = (byte) temp[j];

			if (color) // accumulate color
				col[j] += p[j];
		}

		if (r_monochrome->integer & mask) // monochrome
			p[0] = p[1] = p[2] = (p[0] + p[1] + p[2]) / 3;

		if (r_invert->integer & mask) { // inverted
			p[0] = 255 - p[0];
			p[1] = 255 - p[1];
			p[2] = 255 - p[2];
		}
	}

	if (color) { // average accumulated colors
		for (i = 0; i < 3; i++)
			col[i] /= (width * height);

		if (r_monochrome->integer & mask)
			col[0] = col[1] = col[2] = (col[0] + col[1] + col[2]) / 3;

		if (r_invert->integer & mask) {
			col[0] = 255 - col[0];
			col[1] = 255 - col[1];
			col[2] = 255 - col[2];
		}

		for (i = 0; i < 3; i++)
			color[i] = col[i] / 255.0;
	}
}

static int upload_width, upload_height; // after power-of-two scale

/*
 * R_UploadImage32
 */
static void R_UploadImage32(unsigned *data, int width, int height,
		vec3_t color, r_image_type_t type) {
	unsigned *scaled;
	boolean_t mipmap;

	for (upload_width = 1; upload_width < width; upload_width <<= 1)
		;
	for (upload_height = 1; upload_height < height; upload_height <<= 1)
		;

	// don't ever bother with > MAX_TEXTURE_SIZE textures
	if (upload_width > MAX_TEXTURE_SIZE)
		upload_width = MAX_TEXTURE_SIZE;
	if (upload_height > MAX_TEXTURE_SIZE)
		upload_height = MAX_TEXTURE_SIZE;

	if (upload_width < 1)
		upload_width = 1;
	if (upload_height < 1)
		upload_height = 1;

	mipmap = (type != it_font && type != it_pic && type != it_sky);

	// some images need very little attention (pics, fonts, etc..)
	if (!mipmap && upload_width == width && upload_height == height) {

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_max);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, upload_width, upload_height, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, data);
		return;
	}

	scaled = data;

	if (upload_width != width || upload_height != height) { // whereas others need to be scaled
		scaled = (unsigned *) Z_Malloc(
				upload_width * upload_height * sizeof(unsigned));
		R_ScaleTexture(data, width, height, scaled, upload_width, upload_height);
	}

	if (type == it_effect || type == it_world || type == it_material || type
			== it_skin) // and filtered
		R_FilterTexture((byte *) scaled, upload_width, upload_height, color,
				type);

	if (mipmap) { // and mipmapped
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
				r_filter_aniso);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_max);
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, upload_width, upload_height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, scaled);

	if (scaled != data)
		Z_Free(scaled);
}

/*
 * R_UploadImage
 *
 * This is also used as an entry point for the generated r_notexture.
 */
r_image_t *R_UploadImage(const char *name, void *data, int width, int height,
		r_image_type_t type) {
	r_image_t *image;
	int i;

	// find a free image_t
	for (i = 0, image = r_images; i < r_num_images; i++, image++) {
		if (!image->texnum)
			break;
	}
	if (i == r_num_images) {
		if (r_num_images == MAX_GL_TEXTURES) {
			Com_Warn("R_UploadImage: MAX_GL_TEXTURES reached.\n");
			return r_null_image;
		}
		r_num_images++;
	}
	image = &r_images[i];

	strncpy(image->name, name, MAX_QPATH);
	image->width = width;
	image->height = height;
	image->type = type;

	image->texnum = i + 1;
	R_BindTexture(image->texnum);

	R_UploadImage32((unsigned *) data, width, height, image->color, type);

	image->upload_width = upload_width; // after power of 2 and scales
	image->upload_height = upload_height;

	return image;
}

static const char *nm_suffix[] = { // normalmap texture suffixes
		"nm", "norm", "local", NULL };

/*
 * R_LoadImage
 */
r_image_t *R_LoadImage(const char *name, r_image_type_t type) {
	r_image_t *image;
	char n[MAX_QPATH];
	char nm[MAX_QPATH];
	SDL_Surface *surf;
	int i;

	if (!name || !name[0])
		return r_null_image;

	StripExtension(name, n);

	// see if it's already loaded
	for (i = 0, image = r_images; i < r_num_images; i++, image++) {
		if (!strcmp(n, image->name))
			return image;
	}

	// attempt to load the image
	if (Img_LoadImage(n, &surf)) {

		image = R_UploadImage(n, surf->pixels, surf->w, surf->h, type);

		SDL_FreeSurface(surf);

		if (type == it_world) { // load normalmap image

			for (i = 0; nm_suffix[i] != NULL; i++) {

				snprintf(nm, sizeof(nm), "%s_%s", n, nm_suffix[i]);
				image->normalmap = R_LoadImage(nm, it_normalmap);

				if (image->normalmap != r_null_image)
					break;

				image->normalmap = NULL;
			}
		}
	} else {
		Com_Debug("R_LoadImage: Couldn't load %s\n", n);
		image = r_null_image;
	}

	return image;
}

/*
 * R_InitParticleTextures
 */
static void R_InitParticleTextures(void) {
	int i;

	r_particle_image = R_LoadImage("particles/particle.tga", it_effect);
	r_explosion_image = R_LoadImage("particles/explosion.tga", it_effect);
	r_teleport_image = R_LoadImage("particles/teleport.tga", it_effect);
	r_smoke_image = R_LoadImage("particles/smoke.tga", it_effect);
	r_steam_image = R_LoadImage("particles/steam.tga", it_effect);
	r_bubble_image = R_LoadImage("particles/bubble.tga", it_effect);
	r_rain_image = R_LoadImage("particles/rain.tga", it_effect);
	r_snow_image = R_LoadImage("particles/snow.tga", it_effect);
	r_beam_image = R_LoadImage("particles/beam.tga", it_effect);
	r_burn_image = R_LoadImage("particles/burn.tga", it_effect);
	r_blood_image = R_LoadImage("particles/blood.tga", it_effect);
	r_lightning_image = R_LoadImage("particles/lightning.tga", it_effect);
	r_rail_trail_image = R_LoadImage("particles/railtrail.tga", it_effect);
	r_flame_image = R_LoadImage("particles/flame.tga", it_effect);
	r_spark_image = R_LoadImage("particles/spark.tga", it_effect);

	for (i = 0; i < NUM_BULLET_IMAGES; i++)
		r_bullet_images[i] = R_LoadImage(va("particles/bullet_%i.tga", i),
				it_effect);
}

/*
 * R_InitEnvmapTextures
 */
static void R_InitEnvmapTextures(void) {
	int i;

	for (i = 0; i < NUM_ENVMAP_IMAGES; i++)
		r_envmap_images[i] = R_LoadImage(va("envmaps/envmap_%i.tga", i),
				it_effect);
}

/*
 * R_InitFlareTextures
 */
static void R_InitFlareTextures(void) {
	int i;

	for (i = 0; i < NUM_FLARE_IMAGES; i++)
		r_flare_images[i]
				= R_LoadImage(va("flares/flare_%i.tga", i), it_effect);
}

#define WARP_SIZE 16

/*
 * R_InitWarpTexture
 */
static void R_InitWarpTexture(void) {
	byte warp[WARP_SIZE][WARP_SIZE][4];
	int i, j;

	for (i = 0; i < WARP_SIZE; i++) {
		for (j = 0; j < WARP_SIZE; j++) {
			warp[i][j][0] = rand() % 255;
			warp[i][j][1] = rand() % 255;
			warp[i][j][2] = rand() % 48;
			warp[i][j][3] = rand() % 48;
		}
	}

	r_warp_image = R_UploadImage("***r_warptexture***", (void *) warp, 16, 16,
			it_effect);
}

/*
 * R_InitImages
 */
void R_InitImages(void) {
	unsigned data[256];

	memset(&data, 255, sizeof(data));

	r_null_image = R_UploadImage("***r_notexture***", (void *) data, 16, 16,
			it_null);

	Img_InitPalette();

	R_InitParticleTextures();

	R_InitEnvmapTextures();

	R_InitFlareTextures();

	R_InitWarpTexture();
}

/*
 * R_FreeImage
 */
void R_FreeImage(r_image_t *image) {

	if (!image || !image->texnum)
		return;

	glDeleteTextures(1, &image->texnum);
	memset(image, 0, sizeof(r_image_t));
}

/*
 * R_FreeImages
 */
void R_FreeImages(void) {
	int i;
	r_image_t *image;

	for (i = 0, image = r_images; i < r_num_images; i++, image++) {

		if (!image->texnum)
			continue;

		if (image->type < it_world)
			continue; // keep it

		R_FreeImage(image);
	}
}

/*
 * R_ShutdownImages
 */
void R_ShutdownImages(void) {
	int i;
	r_image_t *image;

	for (i = 0, image = r_images; i < r_num_images; i++, image++)
		R_FreeImage(image);

	r_num_images = 0;
}
