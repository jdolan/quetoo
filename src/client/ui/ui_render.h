/**
 * @file m_render.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_MENU_M_RENDER_H
#define CLIENT_MENU_M_RENDER_H

#include "ui_nodes.h"
#include "renderer/renderer.h"

const r_image_t *MN_LoadImage(const char *name);

void MN_DrawNormImage(float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, const r_image_t *image);
const r_image_t *MN_DrawNormImageByName(float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, const char *name);

void MN_DrawPanel(const vec2_t pos, const vec2_t size, const char *texture, int texX, int texY, const int panelDef[6]);
void MN_DrawFill(int x, int y, int w, int h, int align, const vec4_t color);
void MN_DrawRect(int x, int y, int w, int h, const vec4_t color, float lineWidth, int pattern);
int MN_DrawStringInBox(const menuNode_t *node, int align, int x, int y, int width, int height, const char *text, longlines_t method);
int MN_DrawString(const char *fontID, int align, int x, int y, int absX, int absY, int maxWidth, int maxHeight, const int lineHeight, const char *c, int box_height, int scroll_pos, int *cur_line, boolean_t increaseLine, longlines_t method);

#endif
