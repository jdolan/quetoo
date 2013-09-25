/*
 Copyright (C) 1999-2007 id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "monitor.h"
#include "net_tcp.h"

#ifdef _WIN32
#define LIBXML_STATIC
#endif

#include <libxml2/libxml/tree.h>

#define MAX_MON_MSG_SIZE 2048

typedef struct {
	int32_t socket;

	size_buf_t message;
	byte buffer[MAX_MON_MSG_SIZE];

	xmlDocPtr doc;
} mon_state_t;

static mon_state_t mon_state;

#define xmlString(s) ((const xmlChar *) s)
#define xmlStringf(...) xmlString(va(__VA_ARGS__))

/*
 * @brief Sends the specified (XML) string to the stream.
 */
static _Bool Mon_SendString(const xmlChar *string) {

	const char *s = (const char *) string;
	if (strlen(s) && mon_state.socket) {

		Sb_Clear(&mon_state.message);
		Msg_WriteString(&mon_state.message, s);

		const void *data = (const void *) mon_state.message.data;
		const size_t len = mon_state.message.size;

		return Net_SendStream(mon_state.socket, data, len);
	}

	return false;
}

/*
 * @brief Sends the specified XML node to the stream.
 */
static _Bool Mon_SendXML(xmlNodePtr node) {

	if (node) {
		xmlAddChild(mon_state.doc->children, node);

		xmlChar *string = xmlNodeGetContent(node);

		const _Bool success = Mon_SendString(string);

		xmlFree(string);

		return success;
	}

	return false;
}

/*
 * @brief Sends a brush selection to GtkRadiant for debugging.
 */
void Mon_SendSelect(const char *msg, uint16_t ent_num, uint16_t brush_num, err_t error) {

	xmlNodePtr select = xmlNewNode(NULL, xmlString("select"));
	xmlNodeSetContent(select, xmlStringf("Entity %u, Brush %u: %s", ent_num, brush_num, msg));
	xmlSetProp(select, xmlString("level"), xmlStringf("%d", error));

	xmlNodePtr brush = xmlNewNode(NULL, xmlString("brush"));
	xmlNodeSetContent(brush, xmlStringf("%u %u", ent_num, brush_num));
	xmlAddChild(select, brush);

	Mon_SendXML(select);
}

/*
 * @brief Sends a positional vector to GtkRadiant for debugging.
 */
void Mon_SendPosition(const char *msg, const vec3_t pos, err_t error) {

	xmlNodePtr point_msg = xmlNewNode(NULL, xmlString("pointmsg"));
	xmlNodeSetContent(point_msg, xmlString(msg));
	xmlSetProp(point_msg, xmlString("level"), xmlStringf("%d", error));

	xmlNodePtr point = xmlNewNode(NULL, xmlString("point"));
	xmlNodeSetContent(point, xmlStringf("(%g %g %g", pos[0], pos[1], pos[2]));
	xmlAddChild(point_msg, point);

	Mon_SendXML(point_msg);
}

/*
 * @brief Sends a winding to GtkRadiant for selection.
 */
void Mon_SendWinding(const char *msg, const vec3_t *p, uint16_t num_points, err_t error) {

	xmlNodePtr winding_msg = xmlNewNode(NULL, xmlString("windingmsg"));
	xmlNodeSetContent(winding_msg, xmlString(msg));
	xmlSetProp(winding_msg, xmlString("level"), xmlStringf("%d", error));

	xmlNodePtr winding = xmlNewNode(NULL, xmlString("winding"));
	xmlNodeSetContent(winding, xmlStringf("%u", num_points));
	xmlAddChild(winding_msg, winding);

	int32_t i;
	for (i = 0; i < num_points; i++) {
		xmlNodeAddContent(winding, xmlStringf("(%g %g %g)", p[i][0], p[i][1], p[i][2]));
	}

	Mon_SendXML(winding_msg);
}

/*
 * @brief Initialize BSP monitoring facilities (XML over TCP).
 */
void Mon_Init(const char *host) {

	Net_Init();

	memset(&mon_state, 0, sizeof(mon_state));

	if ((mon_state.socket = Net_Connect(host))) {
		Sb_Init(&mon_state.message, mon_state.buffer, sizeof(mon_state.buffer));

		Com_Print("Connected to %s\n", host);

		Mon_SendString(xmlString("<?xml version=\"1.0\"?><q3map_feedback version=\"1\">"));

		mon_state.doc = xmlNewDoc(xmlString("1.0"));
		mon_state.doc->children = xmlNewDocRawNode(mon_state.doc, NULL,
				xmlString("q3map_feedback"), NULL);
	}
}

/*
 * @brief Shuts down BSP monitoring facilities.
 */
void Mon_Shutdown() {

	if (mon_state.socket) {
		Net_CloseSocket(mon_state.socket);
	}

	if (mon_state.doc) {
		xmlFreeDoc(mon_state.doc);
	}

	Net_Shutdown();
}
