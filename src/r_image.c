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

#include "renderer.h"

image_t *r_notexture;  // use for bad textures
image_t *r_particletexture;  // little dot for particles
image_t *r_explosiontexture;  // expanding explosion particle
image_t *r_teleporttexture;  // teleport ring particle
image_t *r_smoketexture;  // smoke for rocket/grenade trails
image_t *r_bubbletexture;  // bubble trails under water
image_t *r_raintexture;  // atmospheric rain
image_t *r_snowtexture;  // and snow effects
image_t *r_beamtexture;  // rail trail beams
image_t *r_burntexture;  // burn marks from hyperblaster
image_t *r_shadowtexture;  // blob shadows
image_t *r_bloodtexture;  // blood mist
image_t *r_lightningtexture;  // lightning particles
image_t *r_railtrailtexture;  // rail spiral
image_t *r_flametexture;  // flames
image_t *r_bullettextures[NUM_BULLETTEXTURES];  // bullets hitting walls

image_t *r_envmaptextures[NUM_ENVMAPTEXTURES];  // generic environment map

image_t *r_flaretextures[NUM_FLARETEXTURES];  // lense flares

image_t *r_warptexture;  // fragment program warping

image_t r_images[MAX_GL_TEXTURES];
int r_numimages;

GLint r_filter_min = GL_LINEAR_MIPMAP_NEAREST;
GLint r_filter_max = GL_LINEAR;
GLfloat r_filter_aniso = 1.0;

typedef struct {
	const char *name;
	int minimize, maximize;
} r_texturemode_t;

r_texturemode_t r_texturemodes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

#define NUM_GL_TEXTUREMODES (sizeof(r_texturemodes) / sizeof(r_texturemode_t))


/*
R_TextureMode
*/
void R_TextureMode(const char *mode){
	image_t *image;
	int i;

	for(i = 0; i < NUM_GL_TEXTUREMODES; i++){
		if(!strcasecmp(r_texturemodes[i].name, mode))
			break;
	}

	if(i == NUM_GL_TEXTUREMODES){
		Com_Warn("R_TextureMode: Bad filter name.\n");
		return;
	}

	r_filter_min = r_texturemodes[i].minimize;
	r_filter_max = r_texturemodes[i].maximize;

	if(r_anisotropy->value)
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_filter_aniso);
	else
		r_filter_aniso = 1.0;

	// change all the existing mipmap texture objects
	for(i = 0, image = r_images; i < r_numimages; i++, image++){

		if(!image->texnum)
			continue;

		if(image->type == it_chars || image->type == it_pic || image->type == it_sky)
			continue;  // no mipmaps

		R_BindTexture(image->texnum);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_max);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_filter_aniso);
	}
}


/*
R_ListImages_f
*/
void R_ListImages_f(void){
	int i;
	image_t *image;
	int texels;

	texels = 0;
	for(i = 0, image = r_images; i < r_numimages; i++, image++){

		if(!image->texnum)
			continue;

		texels += image->upload_width * image->upload_height;

		switch(image->type){
			case it_chars:
				Com_Printf("C");
				break;
			case it_effect:
				Com_Printf("E");
				break;
			case it_world:
				Com_Printf("W");
				break;
			case it_normalmap:
				Com_Printf("N");
				break;
			case it_material:
				Com_Printf("M");
				break;
			case it_sky:
				Com_Printf("K");
				break;
			case it_skin:
				Com_Printf("S");
				break;
			case it_pic:
				Com_Printf("P");
				break;
			default:
				Com_Printf(" ");
				break;
		}

		Com_Printf(" %4ix%4i: %s\n",
					   image->upload_width, image->upload_height, image->name);
	}
	Com_Printf("Total texel count (not counting mipmaps or lightmaps): %i\n", texels);
}


/*
R_Screenshot_f
*/
void R_Screenshot_f(void){
	byte *buffer;
	char picname[MAX_QPATH];
	char checkname[MAX_OSPATH];
	int i, c;
	FILE *f;
	static int last_shot;  // small optimization, don't fopen so many times

	// create the scrnshots directory if it doesn't exist
	snprintf(checkname, sizeof(checkname), "%s/screenshots/", Fs_Gamedir());
	Fs_CreatePath(checkname);

	// find a file name to save it to
	strcpy(picname, "quake2world--.tga");

	for(i = last_shot; i <= 99; i++){
		picname[11] = i / 10 + '0';
		picname[12] = i % 10 + '0';
		snprintf(checkname, sizeof(checkname), "%s/screenshots/%s", Fs_Gamedir(), picname);
		f = fopen(checkname, "rb");
		if(!f)
			break;  // file doesn't exist
		Fs_CloseFile(f);
	}
	if(i == 100){
		Com_Printf( "R_Screenshot_f: Couldn't create a file\n");
		return;
	}

	last_shot = i;

	c = r_state.width * r_state.height * 3;
	buffer = Z_Malloc(c);

	glReadPixels(0, 0, r_state.width, r_state.height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	Img_WriteTGARLE(checkname, (void *)buffer, r_state.width, r_state.height);

	Z_Free(buffer);
	Com_Printf("Wrote %s\n", picname);
}


/*
R_SoftenTexture
*/
void R_SoftenTexture(byte *in, int width, int height){
	byte *out;
	int i, j, k;
	byte *src, *dest;
	byte *u, *d, *l, *r;

	// soften into a copy of the original image, as in-place would be incorrect
	out = (byte *)Z_Malloc(width * height * 4);
	memcpy(out, in, width * height * 4);

	for(i = 1; i < height - 1; i++){
		for(j = 1; j < width - 1; j++){

			src = in + ((i * width) + j) * 4;  // current input pixel

			u = (src - (width * 4));  // and it's neighbors
			d = (src + (width * 4));
			l = (src - (1 * 4));
			r = (src + (1 * 4));

			dest = out + ((i * width) + j) * 4;  // current output pixel

			for(k = 0; k < 4; k++)
				dest[k] = (u[k] + d[k] + l[k] + r[k]) / 4;
		}
	}

	// copy the softened image over the input image, and free it
	memcpy(in, out, width * height * 4);
	Z_Free(out);
}


/*
R_ScaleTexture
*/
static void R_ScaleTexture(const unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight){
	int i, j;
	unsigned frac, fracstep;
	unsigned p1[1024], p2[1024];

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for(i = 0; i < outwidth; i++){
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for(i = 0; i < outwidth; i++){
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for(i = 0; i < outheight; i++, out += outwidth){
		const unsigned *inrow = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		const unsigned *inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);
		frac = fracstep >> 1;
		for(j = 0; j < outwidth; j++){
			const byte *pix1 = (byte *)inrow + p1[j];
			const byte *pix2 = (byte *)inrow + p2[j];
			const byte *pix3 = (byte *)inrow2 + p1[j];
			const byte *pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte *)(out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte *)(out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte *)(out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}


/*
R_FilterTexture

Applies brightness and contrast to the specified image while optionally computing
the image's average color.  Also handles image inversion and monochrome.  This is
all munged into one function to reduce loops on level load.
*/
void R_FilterTexture(unsigned *in, int width, int height, vec3_t color, imagetype_t type){
	vec3_t intensity, luminosity, temp;
	int i, j, c, mask;
	unsigned col[3];
	byte *p;
	float max, d;

	p = (byte *)in;
	c = width * height;

	mask = 0;  // monochrome/invert

	if(type == it_world || type == it_effect || type == it_material)
		mask = 1;
	else if(type == it_lightmap)
		mask = 2;

	if(color)  // compute average color
		VectorClear(col);

	VectorSet(luminosity, 0.2125, 0.7154, 0.0721);

	for(i = 0; i < c; i++, p+= 4){

		VectorScale(p, 1.0 / 255.0, temp);  // convert to float

		if(type == it_lightmap)  // apply brightness
			VectorScale(temp, r_modulate->value, temp);
		else
			VectorScale(temp, r_brightness->value, temp);

		max = 0.0;  // determine brightest component

		for(j = 0; j < 3; j++){

			if(temp[j] > max)
				max = temp[j];

			if(temp[j] < 0.0)  // enforcing positive values
				temp[j] = 0.0;
		}

		if(max > 1.0)  // clamp without changing hue
			VectorScale(temp, 1.0 / max, temp);

		for(j = 0; j < 3; j++){  // apply contrast

			temp[j] -= 0.5;  // normalize to -0.5 through 0.5

			temp[j] *= r_contrast->value;  // scale

			temp[j] += 0.5;

			if(temp[j] > 1.0)  // clamp
				temp[j] = 1.0;
			else if(temp[j] < 0)
				temp[j] = 0;
		}

		// finally saturation, which requires rgb
		d = DotProduct(temp, luminosity);

		VectorSet(intensity, d, d, d);
		VectorMix(intensity, temp, r_saturation->value, temp);

		for(j = 0; j < 3; j++){

			temp[j] *= 255;  // back to byte

			if(temp[j] > 255)  // clamp
				temp[j] = 255;
			else if(temp[j] < 0)
				temp[j] = 0;

			p[j] = (byte)temp[j];

			if(color)  // accumulate color
				col[j] += p[j];
		}

		if((int)r_monochrome->value & mask)  // monochrome
			p[0] = p[1] = p[2] = (p[0] + p[1] + p[2]) / 3;

		if((int)r_invert->value & mask){  // inverted
			p[0] = 255 - p[0];
			p[1] = 255 - p[1];
			p[2] = 255 - p[2];
		}
	}

	if(color){  // average accumulated colors
		for(i = 0; i < 3; i++)
			col[i] /= (width * height);

		if((int)r_monochrome->value & mask)
			col[0] = col[1] = col[2] = (col[0] + col[1] + col[2]) / 3;

		if((int)r_invert->value & mask){
			col[0] = 255 - col[0];
			col[1] = 255 - col[1];
			col[2] = 255 - col[2];
		}

		for(i = 0; i < 3; i++)
			color[i] = col[i] / 255.0;
	}
}


static int upload_width, upload_height;  // after power-of-two scale

/*
R_UploadImage32
*/
static void R_UploadImage32(unsigned *data, int width, int height, vec3_t color, imagetype_t type){
	unsigned *scaled;
	qboolean mipmap;

	for(upload_width = 1; upload_width < width; upload_width <<= 1)
		;
	for(upload_height = 1; upload_height < height; upload_height <<= 1)
		;

	// don't ever bother with > 2048 textures
	if(upload_width > 2048)
		upload_width = 2048;
	if(upload_height > 2048)
		upload_height = 2048;

	if(upload_width < 1)
		upload_width = 1;
	if(upload_height < 1)
		upload_height = 1;

	mipmap = (type != it_chars && type != it_pic && type != it_sky);

	// some images need very little attention (pics, fonts, etc..)
	if(!mipmap && upload_width == width && upload_height == height){

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_max);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, upload_width, upload_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		return;
	}

	scaled = data;

	if(upload_width != width || upload_height != height){  // whereas others need to be scaled
		scaled = (unsigned *)Z_Malloc(upload_width * upload_height * sizeof(unsigned));
		R_ScaleTexture(data, width, height, scaled, upload_width, upload_height);
	}

	if(type == it_effect || type == it_world || type == it_material || type == it_skin)  // and filtered
		R_FilterTexture(scaled, upload_width, upload_height, color, type);

	if(mipmap){  // and mipmapped
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_filter_aniso);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	}
	else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_max);
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, upload_width, upload_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);

	if(scaled != data)
		Z_Free(scaled);
}


/*
R_UploadImage

This is also used as an entry point for the generated r_notexture.
*/
image_t *R_UploadImage(const char *name, void *data, int width, int height, imagetype_t type){
	image_t *image;
	int i;

	// find a free image_t
	for(i = 0, image = r_images; i < r_numimages; i++, image++){
		if(!image->texnum)
			break;
	}
	if(i == r_numimages){
		if(r_numimages == MAX_GL_TEXTURES){
			Com_Warn("R_UploadImage: MAX_GL_TEXTURES reached.\n");
			return r_notexture;
		}
		r_numimages++;
	}
	image = &r_images[i];

	strncpy(image->name, name, MAX_QPATH);
	image->width = width;
	image->height = height;
	image->type = type;

	image->texnum = i + 1;
	R_BindTexture(image->texnum);

	R_UploadImage32((unsigned *)data, width, height, image->color, type);

	image->upload_width = upload_width;  // after power of 2 and scales
	image->upload_height = upload_height;

	return image;
}


static const char *nm_suffix[] = {  // normalmap texture suffixes
	"nm", "norm", "local", NULL
};

/*
R_LoadImage
*/
image_t *R_LoadImage(const char *name, imagetype_t type){
	image_t *image;
	char n[MAX_QPATH];
	char nm[MAX_QPATH];
	SDL_Surface *surf;
	int i;

	if(!name || !name[0])
		return r_notexture;

	Com_StripExtension(name, n);

	// see if it's already loaded
	for(i = 0, image = r_images; i < r_numimages; i++, image++){
		if(!strcmp(n, image->name))
			return image;
	}

	// attempt to load the image
	if(Img_LoadImage(n, &surf)){

		image = R_UploadImage(n, surf->pixels, surf->w, surf->h, type);

		SDL_FreeSurface(surf);

		if(type == it_world){  // load normalmap image

			for(i = 0; nm_suffix[i] != NULL; i++){

				snprintf(nm, sizeof(nm), "%s_%s", n, nm_suffix[i]);
				image->normalmap = R_LoadImage(nm, it_normalmap);

				if(image->normalmap != r_notexture)
					break;

				image->normalmap = NULL;
			}
		}
	}
	else {
		Com_Dprintf("R_LoadImage: Couldn't load %s\n", n);
		image = r_notexture;
	}

	return image;
}


/*
R_InitParticleTextures
*/
static void R_InitParticleTextures(void){
	int i;

	r_particletexture = R_LoadImage("particles/particle.tga", it_effect);
	r_explosiontexture = R_LoadImage("particles/explosion.tga", it_effect);
	r_teleporttexture = R_LoadImage("particles/teleport.tga", it_effect);
	r_smoketexture = R_LoadImage("particles/smoke.tga", it_effect);
	r_bubbletexture = R_LoadImage("particles/bubble.tga", it_effect);
	r_raintexture = R_LoadImage("particles/rain.tga", it_effect);
	r_snowtexture = R_LoadImage("particles/snow.tga", it_effect);
	r_beamtexture = R_LoadImage("particles/beam.tga", it_effect);
	r_burntexture = R_LoadImage("particles/burn.tga", it_effect);
	r_shadowtexture = R_LoadImage("particles/shadow.tga", it_effect);
	r_bloodtexture = R_LoadImage("particles/blood.tga", it_effect);
	r_lightningtexture = R_LoadImage("particles/lightning.tga", it_effect);
	r_railtrailtexture = R_LoadImage("particles/railtrail.tga", it_effect);
	r_flametexture = R_LoadImage("particles/flame.tga", it_effect);

	for(i = 0; i < NUM_BULLETTEXTURES; i++)
		r_bullettextures[i] = R_LoadImage(va("particles/bullet_%i.tga", i), it_effect);
}


/*
R_InitEnvmapTextures
*/
static void R_InitEnvmapTextures(void){
	int i;

	for(i = 0; i < NUM_ENVMAPTEXTURES; i++)
		r_envmaptextures[i] = R_LoadImage(va("envmaps/envmap_%i.tga", i), it_effect);
}


/*
R_InitFlareTextures
*/
static void R_InitFlareTextures(void){
	int i;

	for(i = 0; i < NUM_FLARETEXTURES; i++)
		r_flaretextures[i] = R_LoadImage(va("flares/flare_%i.tga", i), it_effect);
}


#define WARP_SIZE 16

/*
R_InitWarpTexture
*/
static void R_InitWarpTexture(void){
	byte warp[WARP_SIZE][WARP_SIZE][4];
	int i, j;

	for(i = 0; i < WARP_SIZE; i++){
		for(j = 0; j < WARP_SIZE; j++){
			warp[i][j][0] = rand() % 255;
			warp[i][j][1] = rand() % 255;
			warp[i][j][2] = rand() % 48;
			warp[i][j][3] = rand() % 48;
		}
	}

	r_warptexture = R_UploadImage("***r_warptexture***", (void *)warp, 16, 16, it_effect);
}


/*
R_InitImages
*/
void R_InitImages(void){
	unsigned data[256];

	memset(&data, 255, sizeof(data));

	r_notexture = R_UploadImage("***r_notexture***", (void *)data, 16, 16, it_effect);

	Img_InitPalette();

	R_InitParticleTextures();

	R_InitEnvmapTextures();

	R_InitFlareTextures();

	R_InitWarpTexture();
}


/*
R_FreeImage
*/
void R_FreeImage(image_t *image){

	if(!image || !image->texnum)
		return;

	glDeleteTextures(1, &image->texnum);
	memset(image, 0, sizeof(image_t));
}


/*
R_FreeImages
*/
void R_FreeImages(void){
	int i;
	image_t *image;

	for(i = 0, image = r_images; i < r_numimages; i++, image++){

		if(!image->texnum)
			continue;

		if(image->type < it_world)
			continue;  // keep it

		R_FreeImage(image);
	}
}


/*
R_ShutdownImages
*/
void R_ShutdownImages(void){
	int i;
	image_t *image;

	for(i = 0, image = r_images; i < r_numimages; i++, image++)
		R_FreeImage(image);

	r_numimages = 0;
}
