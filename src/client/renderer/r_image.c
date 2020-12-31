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

#include "r_local.h"

/**
 * @brief Texture sampling modes.
 */
typedef struct {
	/**
	 * @brief The mode name for setting from the console.
	 */
	const char *name;

	/**
	 * @brief The minification and magnification sampling constants.
	 */
	GLenum minify, magnify, minify_no_mip;
} r_texture_mode_t;

static struct {
	/**
	 * @brief The texture sampling mode.
	 */
	r_texture_mode_t texture_mode;

	/**
	 * @brief The texture sampling anisotropy level.
	 */
	GLfloat anisotropy;

} r_image_state;

/**
 * @brief Texture sampling modes.
 */
static const r_texture_mode_t r_texture_modes[] = {
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR, GL_LINEAR },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_LINEAR }
};

/**
 * @brief Sets the texture parameters for mipmapping and anisotropy.
 */
static void R_TextureMode(void) {
	size_t i;

	for (i = 0; i < lengthof(r_texture_modes); i++) {
		if (!g_ascii_strcasecmp(r_texture_modes[i].name, r_texture_mode->string)) {
			r_image_state.texture_mode = r_texture_modes[i];
			break;
		}
	}

	if (i == lengthof(r_texture_modes)) {
		Com_Warn("Bad filter name: %s\n", r_texture_mode->string);
		return;
	}

	if (r_anisotropy->value) {
		if (GLAD_GL_EXT_texture_filter_anisotropic) {
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_image_state.anisotropy);
		} else {
			Com_Warn("Anisotropy is enabled but not supported by your GPU.\n");
			Cvar_ForceSetInteger(r_anisotropy->name, 0);
		}
	} else {
		r_image_state.anisotropy = 1.0;
	}
}

#define MAX_SCREENSHOTS 1000

typedef struct {
	uint32_t width;
	uint32_t height;
	byte *buffer;
} r_screenshot_t;

/**
 * @brief ThreadRunFunc for R_Screenshot_f.
 */
static void R_Screenshot_f_encode(void *data) {
	static int32_t last_screenshot;
	char filename[MAX_QPATH];
	int32_t i;

	for (i = last_screenshot; i < MAX_SCREENSHOTS; i++) {
		g_snprintf(filename, sizeof(filename), "screenshots/quetoo%03u.%s", i, r_screenshot_format->string);

		if (!Fs_Exists(filename)) {
			break;
		}
	}

	if (i == MAX_SCREENSHOTS) {
		Com_Warn("MAX_SCREENSHOTS exceeded\n");
		return;
	}

	last_screenshot = i;

	r_screenshot_t *s = (r_screenshot_t *) data;
	_Bool screenshot_saved;

	if (!g_strcmp0(r_screenshot_format->string, "tga")) {
		screenshot_saved = Img_WriteTGA(filename, s->buffer, s->width, s->height);
	} else {
		screenshot_saved = Img_WritePNG(filename, s->buffer, s->width, s->height);
	}

	if (screenshot_saved) {
		Com_Print("Saved %s\n", Basename(filename));
	} else {
		Com_Warn("Failed to write %s\n", filename);
	}

	Mem_Free(s);
}

/**
* @brief Captures a screenshot, writing it to the user's directory.
*/
void R_Screenshot_f(void) {

	r_screenshot_t *s = Mem_Malloc(sizeof(r_screenshot_t));

	s->width = r_context.drawable_width;
	s->height = r_context.drawable_height;

	s->buffer = Mem_LinkMalloc(s->width * s->height * 3, s);

	glReadPixels(0, 0, s->width, s->height, GL_BGR, GL_UNSIGNED_BYTE, s->buffer);

	Thread_Create(R_Screenshot_f_encode, s, THREAD_NO_WAIT);
}

/**
 * @brief Creates the base image state for the image.
 */
void R_SetupImage(r_image_t *image, GLenum target, GLenum format, GLsizei levels, GLenum type, byte *data) {
	
	assert(image);

	switch (format) {
		case GL_RGB:
		case GL_RGBA:
			break;
		default:
			Com_Error(ERROR_DROP, "Unsupported format %d\n", format);
	}

	if (image->texnum == 0) {
		glGenTextures(1, &(image->texnum));
	}

	glBindTexture(target, image->texnum);
	
	if (image->type & IT_MASK_MIPMAP) {
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, r_image_state.texture_mode.minify);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, r_image_state.texture_mode.magnify);

		if (r_image_state.anisotropy) {
			glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_image_state.anisotropy);
		}
	} else {
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, r_image_state.texture_mode.minify_no_mip);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, r_image_state.texture_mode.magnify);
	}

	if (image->type & IT_MASK_CLAMP_EDGE) {
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	} else {
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_REPEAT);
	}

	if (GLAD_GL_ARB_texture_storage) {
		const GLenum internal_format = (format == GL_RGBA) ? GL_RGBA8 : GL_RGB8;
		if (image->depth) {
			glTexStorage3D(target, levels, internal_format, image->width, image->height, image->depth);

			if (data) {
				glTexSubImage3D(target, 0, 0, 0, 0, image->width, image->height, image->depth, format, type, data);
			}
		} else {
			glTexStorage2D(target, levels, internal_format, image->width, image->height);
			
			if (data) {
				glTexSubImage2D(target, 0, 0, 0, image->width, image->height, format, type, data);
			}
		}
	} else {
		if (image->depth) {
			glTexImage3D(target, 0, format, image->width, image->height, image->depth, 0, format, type, data);
		} else {
			glTexImage2D(target, 0, format, image->width, image->height, 0, format, type, data);
		}
	}

	if ((image->type & IT_MASK_MIPMAP) && data) {
		glGenerateMipmap(target);
	}

	R_RegisterMedia((r_media_t *) image);

	R_GetError(image->media.name);
}

/**
 * @brief Uploads the specified image to the OpenGL implementation. Images that
 * do not have a GL texture reserved (which is most diffusemap textures) will have
 * one generated for them. This flexibility allows for explicitly managed
 * textures (such as lightmaps) to be here as well.
 */
void R_UploadImage(r_image_t *image, GLenum format, byte *data) {

	assert(image);

	GLenum target = GL_TEXTURE_2D;

	if (image->depth) {
		switch (image->type) {
			case IT_MATERIAL:
			case IT_LIGHTMAP:
				target = GL_TEXTURE_2D_ARRAY;
				break;
			case IT_LIGHTGRID:
				target = GL_TEXTURE_3D;
				break;
			default:
				break;
		}
	}
	
	GLsizei levels = 1;

	if (image->type & IT_MASK_MIPMAP) {
		if (image->depth) {
			levels = floorf(log2f(MAX(MAX(image->width, image->height), image->depth))) + 1;
		} else {
			levels = floorf(log2f(MAX(image->width, image->height))) + 1;
		}
	}

	R_SetupImage(image, target, format, levels, GL_UNSIGNED_BYTE, data);
}

/**
 * @brief Retain event listener for images.
 */
_Bool R_RetainImage(r_media_t *self) {

	switch (((r_image_t *) self)->type & ~IT_MASK_FLAGS) {
		case IT_PROGRAM:
		case IT_FONT:
		case IT_UI:
			return true;
		default:
			return false;
	}
}

/**
 * @brief Free event listener for images.
 */
void R_FreeImage(r_media_t *media) {

	r_image_t *image = (r_image_t *) media;

	glDeleteTextures(1, &image->texnum);

	image->texnum = 0;
}

/**
 * @brief Create an image by the specified name.
 */
_Bool R_CreateImage(r_image_t **out, const char *name, const int32_t width, const int32_t height, r_image_type_t type) {
	r_image_t *image;
	char key[MAX_QPATH];

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL name\n");
	}

	StripExtension(name, key);

	if (!(image = (r_image_t *) R_FindMedia(key, R_MEDIA_IMAGE))) {

		image = (r_image_t *) R_AllocMedia(key, sizeof(r_image_t), R_MEDIA_IMAGE);

		image->media.Retain = R_RetainImage;
		image->media.Free = R_FreeImage;

		image->width = width;
		image->height = height;
		image->type = type;
		
		*out = image;
		return true;
	}

	*out = image;
	return false;
}

/**
 * @brief Loads the image by the specified name.
 */
r_image_t *R_LoadImage(const char *name, r_image_type_t type) {
	char key[MAX_QPATH];
	r_image_t *image;

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL name\n");
	}

	StripExtension(name, key);

	if ((image = (r_image_t *) R_FindMedia(key, R_MEDIA_IMAGE))) {
		return image;
	}

	SDL_Surface *surf = Img_LoadSurface(key);
	if (!surf) {

		Com_Debug(DEBUG_RENDERER, "Couldn't load %s\n", key);
		return NULL;
	}
	
	if (!R_CreateImage(&image, name, surf->w, surf->h, type)) {
		return image;
	}

	image->width = surf->w;
	image->height = surf->h;
	image->type = type;

	R_UploadImage(image, GL_RGBA, surf->pixels);

	SDL_FreeSurface(surf);

	return image;
}

#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000

/**
 * @brief Dump the image to the specified output file (must be .png)
 */
void R_DumpImage(const r_image_t *image, const char *output, _Bool mipmap) {

	char real_dir[MAX_OS_PATH], path_name[MAX_OS_PATH];
	Dirname(output, real_dir);
	Fs_Mkdir(real_dir);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	GLenum target = GL_TEXTURE_2D;
	
	if (image->depth) {
		switch (image->type) {
			case IT_MATERIAL:
			case IT_LIGHTMAP:
				target = GL_TEXTURE_2D_ARRAY;
				break;
			case IT_LIGHTGRID:
				target = GL_TEXTURE_3D;
				break;
			default:
				break;
		}
	}

	glBindTexture(target, image->texnum);

	int32_t width, height, depth, mips;

	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &height);
	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_DEPTH, &depth);

	if (image->type & IT_MASK_MIPMAP) {
		glGetTexParameteriv(target, GL_TEXTURE_MAX_LEVEL, &mips);
	} else {
		mips = 0;
	}

	R_GetError("");

	GLubyte *pixels = Mem_Malloc(width * height * depth * 4);

	for (int32_t level = 0; level <= mips; level++) {

		glGetTexImage(target, level, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		if (glGetError() != GL_NO_ERROR) {
			break;
		}

		int32_t scaled_width = width >> level;
		int32_t scaled_height = height >> level;

		if (scaled_width <= 0 || scaled_height <= 0) {
			break;
		}

		for (int32_t d = 0; d < depth; d++) {

			g_strlcpy(path_name, output, sizeof(path_name));

			if (depth > 1) {
				g_strlcat(path_name, va(" layer %i", d), sizeof(path_name));
			}

			if (mips > 0) {
				g_strlcat(path_name, va(" mip %i", level), sizeof(path_name));
			}

			g_strlcat(path_name, ".png", sizeof(path_name));
		
			const char *real_path = Fs_RealPath(path_name);
			SDL_RWops *f = SDL_RWFromFile(real_path, "wb");
			if (!f) {
				return;
			}

			SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(pixels + (scaled_width * scaled_height * 4 * d), scaled_width, scaled_height, 32, scaled_width * 4, RMASK, GMASK, BMASK, AMASK);
			IMG_SavePNG_RW(surf, f, 0);
			SDL_FreeSurface(surf);

			SDL_RWclose(f);
		}

		if (!mipmap) {
			break;
		}
	}

	Mem_Free(pixels);
}

/**
 * @brief R_MediaEnumerator for R_DumpImages_f.
 */
static void R_DumpImages_enumerator(const r_media_t *media, void *data) {

	if (media->type == R_MEDIA_IMAGE) {
		const r_image_t *image = (const r_image_t *) media;
		char path[MAX_OS_PATH];

		g_snprintf(path, sizeof(path), "imgdmp/%s %i", media->name, image->texnum, true);

		R_DumpImage(image, path, true);
	}
}

/**
 * @brief
 */
void R_DumpImages_f(void) {

	Com_Print("Dumping media... ");

	Fs_Mkdir("imgdmp");

	R_EnumerateMedia(R_DumpImages_enumerator, NULL);
}

/**
 * @brief Initializes the images facilities, which includes generation of
 */
void R_InitImages(void) {

	// set up alignment parameters.
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	memset(&r_image_state, 0, sizeof(r_image_state));

	R_TextureMode();

	R_GetError(NULL);
	
	Fs_Mkdir("screenshots");
}
