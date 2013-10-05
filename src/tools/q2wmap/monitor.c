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

#include <libxml/tree.h>

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
 * @brief Duplicate all XML messages to the correct stdio handle.
 */
static void Mon_Stdio(const char *msg, err_t error) {

	switch (error) {
		case ERR_NONE:
			Com_Verbose("%s", msg);
			break;
		case ERR_PRINT:
			Com_Print("%s", msg);
			break;
		case ERR_DROP:
			Com_Warn("%s", msg);
			break;
		case ERR_FATAL:
			Com_Error(error, "%s", msg);
			break;
		default:
			break;
	}
}

/*
 * @brief Sends the specified (XML) string to the stream.
 */
static void Mon_SendString(const xmlChar *string) {

	const char *s = (const char *) string;
	if (strlen(s) && mon_state.socket) {

		Sb_Clear(&mon_state.message);
		Msg_WriteString(&mon_state.message, s);

		const void *data = (const void *) mon_state.message.data;
		const size_t len = mon_state.message.size;

		if (!Net_SendStream(mon_state.socket, data, len)) {
			Com_Error(ERR_FATAL, "@Failed to send \"%s\"", string);
		}
	}
}

/*
 * @brief Sends the specified XML node to the stream.
 */
static void Mon_SendXML(xmlNodePtr node) {

	if (node) {
		xmlAddChild(xmlDocGetRootElement(mon_state.doc), node);

		xmlBufferPtr buffer = xmlBufferCreate();

		if (xmlNodeDump(buffer, mon_state.doc, node, 0, 0) == -1) {
			Com_Warn("Failed to dump node %s", (char *) node->name);
		} else {
			Mon_SendString(xmlBufferContent(buffer));
		}

		xmlBufferFree(buffer);
	}
}

/*
 * @brief Sends a message to GtkRadiant. Do not call this directly; rather, use
 * Com_Print, Com_Warn and friends, which will route through this.
 */
void Mon_SendMessage(const char *msg, err_t error) {

	if (mon_state.doc) {
		xmlNodePtr message = xmlNewNode(NULL, xmlString("message"));
		xmlNodeSetContent(message, xmlString(msg));
		xmlSetProp(message, xmlString("level"), xmlStringf("%d", error));

		Mon_SendXML(message);
	}
}

/*
 * @brief Sends a brush selection to GtkRadiant.
 */
void Mon_SendSelect(const char *msg, uint16_t ent_num, uint16_t brush_num, err_t error) {

	if (mon_state.doc) {
		xmlNodePtr select = xmlNewNode(NULL, xmlString("select"));
		xmlNodeSetContent(select, xmlStringf("Entity %u, Brush %u: %s", ent_num, brush_num, msg));
		xmlSetProp(select, xmlString("level"), xmlStringf("%d", error));

		xmlNodePtr brush = xmlNewNode(NULL, xmlString("brush"));
		xmlNodeSetContent(brush, xmlStringf("%u %u", ent_num, brush_num));
		xmlAddChild(select, brush);

		Mon_SendXML(select);
	}

	Mon_Stdio(va("Entity %u, Brush %u: %s\n", ent_num, brush_num, msg), error);
}

/*
 * @brief Sends a positional vector to GtkRadiant.
 */
void Mon_SendPosition(const char *msg, const vec3_t pos, err_t error) {

	if (mon_state.doc) {
		xmlNodePtr point_msg = xmlNewNode(NULL, xmlString("pointmsg"));
		xmlNodeSetContent(point_msg, xmlString(msg));
		xmlSetProp(point_msg, xmlString("level"), xmlStringf("%d", error));

		xmlNodePtr point = xmlNewNode(NULL, xmlString("point"));
		xmlNodeSetContent(point, xmlStringf("(%g %g %g", pos[0], pos[1], pos[2]));
		xmlAddChild(point_msg, point);

		Mon_SendXML(point_msg);
	}

	Mon_Stdio(va("Point %s: %s\n", vtos(pos), msg), error);
}

/*
 * @brief Sends a winding to GtkRadiant.
 */
void Mon_SendWinding(const char *msg, const vec3_t p[], uint16_t num_points, err_t error) {

	if (mon_state.doc) {
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

	Mon_Stdio(va("Winding at %s: %s\n", vtos(p[0]), msg), error);
}

/*
 * @brief Initialize BSP monitoring facilities (XML over TCP).
 */
void Mon_Init(const char *host) {

	Net_Init();

	memset(&mon_state, 0, sizeof(mon_state));

	mon_state.doc = xmlNewDoc(xmlString("1.0"));

	xmlNodePtr root = xmlNewNode(NULL, xmlString("q3map_feedback"));
	xmlSetProp(root, xmlString("version"), xmlString("1.0"));
	xmlDocSetRootElement(mon_state.doc, root);

	if ((mon_state.socket = Net_Connect(host, NULL))) {
		Sb_Init(&mon_state.message, mon_state.buffer, sizeof(mon_state.buffer));

		Com_Print("Connected to %s\n", host);

		// because we stream child nodes on-demand, we must manually duplicate
		// the XML document header and root element
		Mon_SendString(xmlString("<?xml version=\"1.0\"?><q3map_feedback version=\"1\">"));

		// announce ourselves to Radiant
		Com_Print("@Quake2World Map %s %s %s\n", VERSION, __DATE__, BUILD_HOST);
	}
}

/*
 * @brief Shuts down BSP monitoring facilities.
 */
void Mon_Shutdown(const char *msg) {

	if (mon_state.socket) {

		if (msg && (*msg == '@')) {
			Mon_SendMessage(msg, ERR_FATAL);
		}

		Mon_SendString(xmlString("</q3map_feedback>"));
		sleep(1); // sigh..

		Net_CloseSocket(mon_state.socket);
		mon_state.socket = 0;
	}

	if (mon_state.doc) {
		xmlSaveFile("/tmp/q2wmap_feedback.xml", mon_state.doc);

		xmlFreeDoc(mon_state.doc);
		mon_state.doc = NULL;
	}

	Net_Shutdown();
}
