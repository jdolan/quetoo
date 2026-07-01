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
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

/*
 * TODO(#864): the 2D pipeline (console, HUD, fonts) is not yet ported to
 * SDL_gpu. These are stubs preserving the API used by the client and cgame.
 * R_BindFont reports non-zero character dimensions so console/HUD layout math
 * (which divides by them) stays sane; nothing is actually drawn yet.
 */

/**
 * @brief
 */
void R_SetDraw2DProjection(r_draw_2d_projection_t projection) {
}

/**
 * @brief
 */
void R_BindFont(const char *name, int32_t *cw, int32_t *ch) {

  if (cw) { *cw = 8; }
  if (ch) { *ch = 16; }
}

/**
 * @brief
 */
void R_SetClippingFrame(int32_t x, int32_t y, int32_t w, int32_t h) {
}

/**
 * @brief
 */
void R_Draw2DChar(int32_t x, int32_t y, char c, const color_t color) {
}

/**
 * @brief
 */
int32_t R_StringWidth(const char *s) {
  return 0;
}

/**
 * @brief
 */
size_t R_Draw2DString(int32_t x, int32_t y, const char *s, const color_t color) {
  return 0;
}

/**
 * @brief
 */
size_t R_Draw2DBytes(int32_t x, int32_t y, const char *s, size_t size, const color_t color) {
  return 0;
}

/**
 * @brief
 */
size_t R_Draw2DSizedString(int32_t x, int32_t y, const char *s, size_t len, size_t size, const color_t color) {
  return 0;
}

/**
 * @brief
 */
void R_Draw2DImage(int32_t x, int32_t y, int32_t w, int32_t h, const r_image_t *image, const color_t color) {
}

/**
 * @brief
 */
void R_Draw2DFramebuffer(int32_t x, int32_t y, int32_t w, int32_t h, const r_framebuffer_t *framebuffer, const color_t color) {
}

/**
 * @brief
 */
void R_Draw2DFill(int32_t x, int32_t y, int32_t w, int32_t h, const color_t color) {
}

/**
 * @brief
 */
void R_Draw2DLines(const int32_t *points, size_t count, const color_t color) {
}

/**
 * @brief
 */
void R_Draw2D(void) {
}

/**
 * @brief
 */
void R_InitDraw2D(void) {
}

/**
 * @brief
 */
void R_ShutdownDraw2D(void) {
}
