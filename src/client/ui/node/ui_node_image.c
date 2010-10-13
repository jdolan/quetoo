/**
 * @file m_node_image.c
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

#include "ui_nodes.h"
#include "ui_parse.h"
#include "ui_render.h"
#include "ui_node_image.h"
#include "ui_node_abstractnode.h"

#include "client.h"

/**
 * @brief Handled after the end of the load of the node from the script (all data and/or child are set)
 */
static void MN_ImageNodeLoaded (menuNode_t *node)
{
	/* update the size when its possible */
	if (node->size[0] == 0 && node->size[1] == 0) {
		if (node->texl[0] != 0 || node->texh[0]) {
			node->size[0] = node->texh[0] - node->texl[0];
			node->size[1] = node->texh[1] - node->texl[1];
		} else if (node->image) {
			const image_t *image = MN_LoadImage(node->image);
			if (image) {
				node->size[0] = image->width;
				node->size[1] = image->height;
			}
		}
	}
#ifdef DEBUG
	if (node->size[0] == 0 && node->size[1] == 0) {
		if (node->onClick || node->onRightClick || node->onMouseEnter || node->onMouseLeave || node->onWheelUp || node->onWheelDown || node->onWheel || node->onMiddleClick) {
			Com_Debug("Node '%s' is an active image without size\n", MN_GetPath(node));
		}
	}
#endif
}

/**
 * @todo Extract ekg_ into another node behaviour
 * @todo Center image, or use textalign property
 */
void MN_ImageNodeDraw (menuNode_t *node)
{
	vec2_t size;
	vec2_t nodepos;
	const image_t *image;

	const char* imageName = MN_GetReferenceString(node, node->image);
	if (!imageName || imageName[0] == '\0')
		return;

	image = MN_LoadImage(imageName);
	if (!image)
		return;

	MN_GetNodeAbsPos(node, nodepos);
	if (node->size[0] && !node->size[1]) {
		float scale;

		scale = image->width / node->size[0];
		Vector2Set(size, node->size[0], image->height / scale);
	} else if (node->size[1] && !node->size[0]) {
		float scale;

		scale = image->height / node->size[1];
		Vector2Set(size, image->width / scale, node->size[1]);
	} else {
		if (node->preventRatio) {
			/* maximize the image into the bounding box */
			float ratio;
			ratio = (float) image->width / (float) image->height;
			if (node->size[1] * ratio > node->size[0]) {
				Vector2Set(size, node->size[0], node->size[0] / ratio);
			} else {
				Vector2Set(size, node->size[1] * ratio, node->size[1]);
			}
		} else {
			Vector2Copy(node->size, size);
		}
	}
	MN_DrawNormImage(nodepos[0], nodepos[1], size[0], size[1],
		node->texh[0], node->texh[1], node->texl[0], node->texl[1], ALIGN_UL, image);
}

static const value_t properties[] = {
	{"preventratio", V_BOOL, offsetof(menuNode_t, preventRatio), MEMBER_SIZEOF(menuNode_t, preventRatio)},
	/** @todo delete it when its possible (use more button instead of image) */
	{"mousefx", V_BOOL, offsetof(menuNode_t, mousefx), MEMBER_SIZEOF(menuNode_t, mousefx)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterImageNode (nodeBehaviour_t* behaviour)
{
	/** @todo rename it according to the function name when its possible */
	behaviour->name = "pic";
	behaviour->draw = MN_ImageNodeDraw;
	behaviour->loaded = MN_ImageNodeLoaded;
	behaviour->properties = properties;
}
