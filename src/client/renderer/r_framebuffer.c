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
#include "r_gl.h"

r_framebuffer_state_t r_framebuffer_state;

/**
 * @brief Retain event listener for framebuffers.
 * FIXME: ideally I'd like to call fb->color->media.retain here, but the color texture will get
 * freed before the FB will. Maybe we should only free dependencies inside the parent free func?
 */
static _Bool R_RetainFramebuffer(r_media_t *self) {
	r_framebuffer_t *fb = (r_framebuffer_t *) self;

	if (fb->color_type == IT_NULL || fb->color_type == IT_PROGRAM || fb->color_type == IT_FONT || fb->color_type == IT_UI) {
		return true;
	}

	return false;
}

/**
 * @brief Free event listener for images.
 */
static void R_FreeFramebuffer(r_media_t *media) {
	r_framebuffer_t *fb = (r_framebuffer_t *) media;

	if (fb->depth_stencil) {
		glDeleteRenderbuffers(1, &fb->depth_stencil);
	}

	if (r_framebuffer_state.current_framebuffer == fb) {
		R_BindFramebuffer(FRAMEBUFFER_DEFAULT);
	}

	if (fb->framebuffer) {
		glDeleteFramebuffers(1, &fb->framebuffer);
	}

	R_GetError(NULL);
}

/**
 * @brief Create a new, blank framebuffer.
 */
r_framebuffer_t *R_CreateFramebuffer(const char *key) {
	r_framebuffer_t *fb = (r_framebuffer_t *) R_AllocMedia(key, sizeof(r_framebuffer_t), MEDIA_FRAMEBUFFER);

	fb->media.Retain = R_RetainFramebuffer;
	fb->media.Free = R_FreeFramebuffer;

	glGenFramebuffers(1, &fb->framebuffer);

	R_RegisterMedia((r_media_t *) fb);
	R_GetError(NULL);
	return fb;
}

/**
 * @brief Bind a framebuffer and make it active.
 */
void R_BindFramebuffer(const r_framebuffer_t *fb) {

	if (r_framebuffer_state.current_framebuffer == fb) {
		return;
	}

	if (fb) {
		glBindFramebuffer(GL_FRAMEBUFFER, fb->framebuffer);
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	r_framebuffer_state.current_framebuffer = fb;
	R_GetError(NULL);
}

/**
 * @brief Attach the specified media image to the framebuffer. Be careful to free
 * the framebuffer when the image is freed, or you'll have a bad time.
 * This affects currently-bound framebuffer state and currently-bound diffuse texture state.
 */
void R_AttachFramebufferImage(r_framebuffer_t *fb, r_image_t *image) {

	if (fb->color) {
		Com_Error(ERROR_FATAL, "Already has a color attachment");
	}

	R_BindFramebuffer(fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image->texnum, 0);
	fb->color = image;
	fb->color_type = image->type;

	R_RegisterDependency((r_media_t *) fb, (r_media_t *) image);
	R_GetError(NULL);
}

/**
 * @brief Convenience function to create and attach the necessary render buffer required
 * for depth/stencil testing.
 */
void R_CreateFramebufferDepthStencilBuffers(r_framebuffer_t *fb) {

	if (fb->depth_stencil) {
		Com_Error(ERROR_FATAL, "Already has a depth/stencil attachment");
	}

	if (!fb->color) {
		Com_Error(ERROR_FATAL, "Need color attachment for sizing");
	}

	R_BindFramebuffer(fb);

	glGenRenderbuffers(1, &fb->depth_stencil);
	glBindRenderbuffer(GL_RENDERBUFFER, fb->depth_stencil);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fb->color->width, fb->color->height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depth_stencil);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb->depth_stencil);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	R_GetError(NULL);
}

/**
 * @brief See if the framebuffer is ready to be used.
 */
_Bool R_FramebufferReady(const r_framebuffer_t *fb) {
	R_BindFramebuffer(fb);
	return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}