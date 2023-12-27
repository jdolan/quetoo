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
	 * @brief The maximum supported texture sampling anisotropy level.
	 */
	GLfloat max_anisotropy;

	/**
	 * @brief The current anisotropy level, clamped to `max_anisotropy`.
	 */
	GLfloat anisotropy;

	/**
	 * @brief If set, take a screenshot at the end of the frame.
	 */
	r_screenshot_type_t screenshot;
} r_image_state;

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
		case SCREENSHOT_VIEW: {
			if (r_post->value) {
				R_ReadFramebufferAttachment(view->framebuffer, ATTACHMENT_POST, &surface);
			} else {
				R_ReadFramebufferAttachment(view->framebuffer, ATTACHMENT_COLOR, &surface);
			}
		}
			break;
		default:
			surface = SDL_CreateRGBSurfaceWithFormat(0,
													 view->framebuffer->width,
													 view->framebuffer->height,
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
 * @brief Creates the base image state for the image.
 */
void R_SetupImage(r_image_t *image) {
	
	assert(image);
	assert(image->type);
	assert(image->width);
	assert(image->height);
	assert(image->target);
	assert(image->internal_format);
	assert(image->format);
	assert(image->pixel_type);

	if (image->minify == 0) {
		image->minify = GL_LINEAR;
	}

	if (image->magnify == 0) {
		image->magnify = GL_LINEAR;
	}

	if (image->levels == 0) {
		switch (image->type) {
			case IMG_ATLAS:
			case IMG_SPRITE:
			case IMG_MATERIAL:
				image->levels = floorf(log2f(Mini(image->width, image->height))) + 1;
				break;
			default:
				image->levels = 1;
				break;
		}
	}

	if (image->texnum == 0) {
		glGenTextures(1, &(image->texnum));
	}

	glBindTexture(image->target, image->texnum);

	glTexParameteri(image->target, GL_TEXTURE_MIN_FILTER, image->minify);
	glTexParameteri(image->target, GL_TEXTURE_MAG_FILTER, image->magnify);
	glTexParameterf(image->target, GL_TEXTURE_MAX_ANISOTROPY, r_image_state.anisotropy);

	if (image->depth) {
		glTexStorage3D(image->target, image->levels, image->internal_format, image->width, image->height, image->depth);
	} else {
		glTexStorage2D(image->target, image->levels, image->internal_format, image->width, image->height);
	}

	R_RegisterMedia((r_media_t *) image);

	R_GetError(image->media.name);
}

/**
 * @brief Uploads the given pixel data to the specified image and target.
 * @param image The image.
 * @param target The upload target, which may be different from the image's bind target.
 * @param data The pixel data.
 */
void R_UploadImageTarget(r_image_t *image, GLenum target, void *data) {

	assert(image);
	assert(target);

	if (image->texnum == 0) {
		R_SetupImage(image);
	}

	glBindTexture(image->target, image->texnum);

	if (image->depth > 1) {
		glTexSubImage3D(target, 0, 0, 0, 0, image->width, image->height, image->depth, image->format, image->pixel_type, data);
	} else {
		glTexSubImage2D(target, 0, 0, 0, image->width, image->height, image->format, image->pixel_type, data);
	}

	if (image->levels > 1) {
		glGenerateMipmap(image->target);
	}

	R_GetError(image->media.name);
}

/**
 * @brief Uploads the given pixel data to the specified image.
 * @param image The image.
 * @param data The pixel data.
 */
void R_UploadImage(r_image_t *image, void *data) {

	R_UploadImageTarget(image, image->target, data);
}


/**
 * @brief Retain event listener for images.
 */
_Bool R_RetainImage(r_media_t *self) {

	switch (((r_image_t *) self)->type) {
		case IMG_PROGRAM:
		case IMG_FONT:
		case IMG_UI:
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

	if (type == IMG_CUBEMAP) {

		image->width = surface->w / 4;
		image->height = surface->h / 3;
		image->target = GL_TEXTURE_CUBE_MAP;
		image->internal_format = GL_RGB8;
		image->format = GL_RGB;
		image->pixel_type = GL_UNSIGNED_BYTE;

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

			R_UploadImageTarget(image, target, side->pixels);

			SDL_FreeSurface(side);
		}
	} else {
		image->width = surface->w;
		image->height = surface->h;
		image->target = GL_TEXTURE_2D;
		image->internal_format = GL_RGBA8;
		image->format = GL_RGBA;
		image->pixel_type = GL_UNSIGNED_BYTE;

		R_UploadImage(image, surface->pixels);
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

	if (image->type == IMG_CUBEMAP) {
		depth = 6;
	}

	if (image->levels > 1) {
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

	// set up alignment parameters
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	memset(&r_image_state, 0, sizeof(r_image_state));

	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &r_image_state.max_anisotropy);
	r_image_state.anisotropy = Clampf(r_anisotropy->value, 1.f, r_image_state.max_anisotropy);

	R_GetError(NULL);
	
	Fs_Mkdir("screenshots");
}
