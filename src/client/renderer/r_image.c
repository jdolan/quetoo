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

/**
 * @brief Screenshot types.
 */
typedef enum {
	SCREENSHOT_NONE,
	SCREENSHOT_DEFAULT,
	SCREENSHOT_VIEW,
} r_screenshot_type_t;

/**
 * @brief Image state.
 */
static struct {
	/**
	 * @brief The texture sampling mode.
	 */
	r_texture_mode_t texture_mode;

	/**
	 * @brief The texture sampling anisotropy level.
	 */
	GLfloat anisotropy;

	/**
	 * @brief If set, take a screenshot at the end of the frame.
	 */
	r_screenshot_type_t screenshot;
} r_image_state;

/**
 * @brief Texture sampling modes.
 */
static const r_texture_mode_t r_texture_modes[] = {
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR, GL_LINEAR },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST, GL_NEAREST },
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
		r_image_state.texture_mode = r_texture_modes[0];
		return;
	}

	if (r_anisotropy->value) {
		if (GLAD_GL_ARB_texture_filter_anisotropic) {
			GLfloat max_anisotropy;
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);
			r_image_state.anisotropy = Minf(max_anisotropy, r_anisotropy->value);
		} else {
			Com_Warn("Anisotropy is enabled but not supported by your GPU.\n");
			Cvar_ForceSetInteger(r_anisotropy->name, 0);
		}
	} else {
		r_image_state.anisotropy = 1.0;
	}
}

#define MAX_SCREENSHOTS 1000

/**
 * @brief ThreadRunFunc for R_Screenshot.
 */
static void R_Screenshot_encode(void *data) {
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

	SDL_Surface *surface = (SDL_Surface *) data;
	_Bool screenshot_saved;

	if (!g_strcmp0(r_screenshot_format->string, "tga")) {
		screenshot_saved = Img_WriteTGA(filename, surface->pixels, surface->w, surface->h);
	} else {
		screenshot_saved = Img_WritePNG(filename, surface->pixels, surface->w, surface->h);
	}

	if (screenshot_saved) {
		Com_Print("Saved %s\n", Basename(filename));
	} else {
		Com_Warn("Failed to write %s\n", filename);
	}

	SDL_FreeSurface(surface);
}

/**
 * @brief Captures a screenshot, if requested, writing it to the user's directory.
 */
void R_Screenshot(r_view_t *view) {

	SDL_Surface *surface = NULL;

	switch (r_image_state.screenshot) {
		case SCREENSHOT_NONE:
			return;
		case SCREENSHOT_VIEW:
			surface = R_ReadFramebuffer(view->framebuffer);
			break;
		default:
			surface = SDL_CreateRGBSurfaceWithFormat(0,
													 r_context.drawable_width,
													 r_context.drawable_height,
													 24,
													 SDL_PIXELFORMAT_RGB24);

			glReadPixels(0, 0, surface->w, surface->h, GL_BGR, GL_UNSIGNED_BYTE, surface->pixels);
			break;
	}

	assert(surface);

	Thread_Create(R_Screenshot_encode, surface, THREAD_NO_WAIT);

	r_image_state.screenshot = SCREENSHOT_NONE;
}

/**
* @brief Sets the screenshot type to take this frame.
*/
void R_Screenshot_f(void) {

	if (!g_strcmp0(Cmd_Argv(1), "view")) {
		r_image_state.screenshot = SCREENSHOT_VIEW;
	} else {
		r_image_state.screenshot = SCREENSHOT_DEFAULT;
	}
}

/**
 * @brief
 */
static GLenum R_GetInternalImageFormat(const r_image_t *image) {

	switch (image->format) {
		case GL_RED:
			return GL_R8;
		case GL_RGBA:
			return GL_RGBA8;
		case GL_RGB:
			return GL_RGB8;
		default:
			Com_Error(ERROR_DROP, "Unsupported format %d\n", image->format);

	}
}

/**
 * @brief Creates the base image state for the image.
 */
void R_SetupImage(r_image_t *image) {
	
	assert(image);
	assert(image->type);
	assert(image->width);
	assert(image->height);
	assert(image->target);
	assert(image->format);

	if (image->texnum == 0) {
		glGenTextures(1, &(image->texnum));
	}

	glBindTexture(image->target, image->texnum);

	if (image->type & IT_MASK_MIPMAP) {
		glTexParameteri(image->target, GL_TEXTURE_MIN_FILTER, r_image_state.texture_mode.minify);
		glTexParameteri(image->target, GL_TEXTURE_MAG_FILTER, r_image_state.texture_mode.magnify);

		if (r_image_state.anisotropy > 1.f) {
			glTexParameterf(image->target, GL_TEXTURE_MAX_ANISOTROPY, r_image_state.anisotropy);
		}
	} else {
		glTexParameteri(image->target, GL_TEXTURE_MIN_FILTER, r_image_state.texture_mode.minify_no_mip);
		glTexParameteri(image->target, GL_TEXTURE_MAG_FILTER, r_image_state.texture_mode.magnify);
	}

	if (image->type & IT_MASK_CLAMP_EDGE) {
		glTexParameteri(image->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(image->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(image->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	} else {
		glTexParameteri(image->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(image->target, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(image->target, GL_TEXTURE_WRAP_R, GL_REPEAT);
	}

	if (r_texture_storage->integer && GLAD_GL_ARB_texture_storage) {

		GLsizei levels = 1;
		if (image->type & IT_MASK_MIPMAP) {
			if (image->depth && image->target == GL_TEXTURE_3D) {
				levels = floorf(log2f(Mini(Mini(image->width, image->height), image->depth))) + 1;
			} else {
				levels = floorf(log2f(Mini(image->width, image->height))) + 1;
			}
		}
		
		const GLenum internal_format = R_GetInternalImageFormat(image);
		if (image->depth) {
			glTexStorage3D(image->target, levels, internal_format, image->width, image->height, image->depth);
		} else {
			glTexStorage2D(image->target, levels, internal_format, image->width, image->height);
		}
	}

	R_RegisterMedia((r_media_t *) image);

	R_GetError(image->media.name);
}

/**
 * @brief Uploads the given pixel data to the specified image and target.
 * @param image The image.
 * @param target The target, which may be different than the image's bind target.
 * @param data The pixel data.
 */
void R_UploadImage(r_image_t *image, GLenum target, byte *data) {

	assert(image);
	assert(target);
	assert(data);

	if (image->texnum == 0) {
		R_SetupImage(image);
	}

	glBindTexture(image->target, image->texnum);

	if (r_texture_storage->integer && GLAD_GL_ARB_texture_storage) {
		if (image->depth) {
			glTexSubImage3D(target, 0, 0, 0, 0, image->width, image->height, image->depth, image->format, GL_UNSIGNED_BYTE, data);
		} else {
			glTexSubImage2D(target, 0, 0, 0, image->width, image->height, image->format, GL_UNSIGNED_BYTE, data);
		}
	} else {
		const GLenum internal_format = R_GetInternalImageFormat(image);

		if (image->depth) {
			glTexImage3D(target, 0, internal_format, image->width, image->height, image->depth, 0, image->format, GL_UNSIGNED_BYTE, data);
		} else {
			glTexImage2D(target, 0, internal_format, image->width, image->height, 0, image->format, GL_UNSIGNED_BYTE, data);
		}
	}

	if (image->type & IT_MASK_MIPMAP) {
		glGenerateMipmap(image->target);
	}

	R_GetError(image->media.name);
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
 * @brief Loads the image by the specified name.
 */
r_image_t *R_LoadImage(const char *name, r_image_type_t type) {
	char key[MAX_QPATH];
	r_image_t *image;

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL name\n");
	}

	StripExtension(name, key);

	image = (r_image_t *) R_FindMedia(key, R_MEDIA_IMAGE);
	if (image) {
		return image;
	}

	image = (r_image_t *) R_FindMedia(key, R_MEDIA_ATLAS_IMAGE);
	if (image) {
		return image;
	}

	SDL_Surface *surface = Img_LoadSurface(key);

	if (!surface) {
		Com_Debug(DEBUG_RENDERER, "Couldn't load %s\n", key);
		return NULL;
	}

	image = (r_image_t *) R_AllocMedia(key, sizeof(r_image_t), R_MEDIA_IMAGE);

	image->media.Retain = R_RetainImage;
	image->media.Free = R_FreeImage;

	image->type = type;

	if (type == IT_CUBEMAP) {
		image->target = GL_TEXTURE_CUBE_MAP;
		image->format = GL_RGB;
		
		image->width = surface->w / 4;
		image->height = surface->h / 3;

		// right left front back up down
		const vec2s_t offsets[] = {
			Vec2s(2, 1),
			Vec2s(0, 1),
			Vec2s(3, 1),
			Vec2s(1, 1),
			Vec2s(1, 0),
			Vec2s(1, 2)
		};

		const int32_t rotations[] = {
			1,
			3,
			2,
			0,
			0,
			2
		};

		for (size_t i = 0; i < 6; i++) {
			const GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum) i;

			SDL_Surface *side = SDL_CreateRGBSurfaceWithFormat(0, image->width, image->height, 3, SDL_PIXELFORMAT_RGB24);

			SDL_BlitSurface(surface, &(const SDL_Rect) {
				.x = image->width * offsets[i].x,
				.y = image->height * offsets[i].y,
				.w = image->width,
				.h = image->height
			}, side, &(SDL_Rect) {
				.x = 0,
				.y = 0,
				.w = image->width,
				.h = image->height
			});

			if (rotations[i]) {
				SDL_Surface *rotated = Img_RotateSurface(side, rotations[i]);

				if (rotated != side) {
					SDL_FreeSurface(side);
					side = rotated;
				}
			}

			R_UploadImage(image, target, side->pixels);

			SDL_FreeSurface(side);
		}
	} else {
		image->width = surface->w;
		image->height = surface->h;
		image->target = GL_TEXTURE_2D;
		image->format = GL_RGBA;

		R_UploadImage(image, image->target, surface->pixels);
	}
		
	SDL_FreeSurface(surface);

	R_GetError(name);

	return image;
}

/**
 * @brief Dump the image to the specified output file.
 */
static void R_DumpImage(const r_image_t *image, const char *output, _Bool mipmap, _Bool raw) {

	if (image->format != GL_RGB && image->format != GL_RGBA) {
		Com_Warn("Skipped %s due to format\n", image->media.name);
		return;
	}

	char real_dir[MAX_OS_PATH], path_name[MAX_OS_PATH];
	Dirname(output, real_dir);
	Fs_Mkdir(real_dir);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	glBindTexture(image->target, image->texnum);

	int32_t width, height, depth, mips;

	glGetTexLevelParameteriv(image->target, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(image->target, 0, GL_TEXTURE_HEIGHT, &height);
	glGetTexLevelParameteriv(image->target, 0, GL_TEXTURE_DEPTH, &depth);

	if (image->type == IT_CUBEMAP) {
		depth = 6;
	}

	if ((image->type & IT_MASK_MIPMAP) && mipmap) {
		glGetTexParameteriv(image->target, GL_TEXTURE_MAX_LEVEL, &mips);
	} else {
		mips = 0;
	}

	R_GetError("");

	int32_t bpp = (image->format == GL_RGBA ? 4 : 3);
	GLubyte *pixels = Mem_Malloc(width * height * depth * bpp);
	GLubyte *pixels_start = pixels;

	for (int32_t level = 0; level <= mips; level++) {

		glGetTexImage(image->target, level, image->format, GL_UNSIGNED_BYTE, pixels);

		if (glGetError() != GL_NO_ERROR) {
			break;
		}

		int32_t scaled_width, scaled_height;

		glGetTexLevelParameteriv(image->target, level, GL_TEXTURE_WIDTH, &scaled_width);
		glGetTexLevelParameteriv(image->target, level, GL_TEXTURE_HEIGHT, &scaled_height);

		if (scaled_width <= 0 || scaled_height <= 0) {
			break;
		}

		for (int32_t d = 0; d < depth; d++) {

			g_strlcpy(path_name, output, sizeof(path_name));

			g_strlcat(path_name, va("_%ix%i", width, height), sizeof(path_name));
				
			if (depth > 1) {
				g_strlcat(path_name, va("x%i", d), sizeof(path_name));
			}

			if (mips > 0) {
				g_strlcat(path_name, va("_%i", level), sizeof(path_name));
			}
				
			if (raw) {
			
				file_t *f = Fs_OpenWrite(path_name);

				if (!f) {
					break;
				}
				
				Fs_Write(f, pixels, bpp, scaled_width * scaled_height);

				Fs_Close(f);
			} else {
				g_strlcat(path_name, ".png", sizeof(path_name));
		
				const char *real_path = Fs_RealPath(path_name);

				SDL_RWops *f = SDL_RWFromFile(real_path, "wb");

				if (!f) {
					break;
				}

				SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(pixels, scaled_width, scaled_height, bpp == 3 ? 24 : 32, scaled_width * bpp, bpp == 3 ? SDL_PIXELFORMAT_RGB24 : SDL_PIXELFORMAT_RGBA32);

				IMG_SavePNG_RW(surf, f, 0);

				SDL_FreeSurface(surf);

				SDL_RWclose(f);
			}

			pixels += scaled_width * scaled_height * bpp;
		}

		pixels = pixels_start;
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

		g_snprintf(path, sizeof(path), "imgdmp/%i", image->texnum);

		R_DumpImage(image, path, true, false);
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
 * @brief Initializes the images facilities
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
