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

#include <SDL_timer.h>

#include "monitor.h"

#if !defined(NOMONITOR)
#include "net/net_tcp.h"
#include "net/net_message.h"

#if defined(_WIN32)
	#define LIBXML_STATIC
#endif

#include <libxml/tree.h>

typedef struct {
	int32_t socket;

	mem_buf_t message;
	byte buffer[MAX_MSG_SIZE];

	xmlDocPtr doc;
} mon_state_t;

static mon_state_t mon_state;
static GList *mon_backlog; // nodes created before a connection was established

#define xmlString(s) ((const xmlChar *) s)
#define xmlStringf(...) xmlString(va(__VA_ARGS__))
#endif

/**
 * @brief Sends the specified (XML) string to the stream.
 */
static void Mon_SendString(const xmlChar *string) {

	const char *s = (const char *) string;
	if (strlen(s) && mon_state.socket) {

		Mem_ClearBuffer(&mon_state.message);
		Net_WriteString(&mon_state.message, s);

		const void *data = (const void *) mon_state.message.data;
		const size_t len = mon_state.message.size;

		if (!Net_SendStream(mon_state.socket, data, len)) {
			Com_Error(ERROR_FATAL, "@Failed to send \"%s\"", string);
		}
	}
}

/**
 * @brief Sends the specified XML node to the stream.
 */
static void Mon_SendXML(xmlNodePtr node) {

	if (node) {
		if (mon_state.doc) {
			xmlAddChild(xmlDocGetRootElement(mon_state.doc), node);

			xmlBufferPtr buffer = xmlBufferCreate();

			if (xmlNodeDump(buffer, mon_state.doc, node, 0, 0) == -1) {
				Com_Warn("@Failed to dump node %s", (char *) node->name);
			} else {
				Mon_SendString(xmlBufferContent(buffer));
			}

			xmlBufferFree(buffer);
		} else {
			mon_backlog = g_list_append(mon_backlog, node);
		}
	}
}

/**
 * @brief Sends a message to GtkRadiant. Note that Com_Print, Com_Warn and
 * friends will route through this so that stdout and stderr are duplicated to
 * GtkRadiant.
 */
void Mon_SendMessage(mon_level_t level, const char *msg) {

	xmlNodePtr message = xmlNewNode(NULL, xmlString("message"));
	xmlNodeSetContent(message, xmlString(msg));
	xmlSetProp(message, xmlString("level"), xmlStringf("%d", level));

	Mon_SendXML(message);
}

/**
 * @brief Routes all XML-originating messages (Mon_Send*) to the appropriate stdio routines,
 * escaping them to avoid infinite loops.
 */
static void Mon_Stdio(mon_level_t level, const char *msg) {
	switch (level) {
		case MON_PRINT:
			Com_Print("@%s\n", msg);
			break;
		case MON_WARN:
			Com_Warn("!@%s\n", msg);
			break;
		case MON_ERROR:
			Com_Error(ERROR_FATAL, "!@%s\n", msg);
		default:
			break;
	}
}

/**
 * @brief Sends a brush selection to GtkRadiant.
 */
void Mon_SendSelect_(const char *func, mon_level_t level, int32_t e, int32_t b, const char *msg) {

	xmlNodePtr select = xmlNewNode(NULL, xmlString("select"));
	xmlNodeSetContent(select, xmlStringf("%s: Entity %u, Brush %u: %s", func, e, b, msg));
	xmlSetProp(select, xmlString("level"), xmlStringf("%d", level));

	xmlNodePtr brush = xmlNewNode(NULL, xmlString("brush"));
	xmlNodeSetContent(brush, xmlStringf("%u %u", e, b));
	xmlAddChild(select, brush);

	Mon_SendXML(select);

	Mon_Stdio(level, va("%s: Entity %u, Brush %u: %s", func, e, b, msg));
}

/**
 * @brief Sends a positional vector to GtkRadiant.
 */
void Mon_SendPoint_(const char *func, mon_level_t level, const vec3_t p, const char *msg) {

	xmlNodePtr point_msg = xmlNewNode(NULL, xmlString("pointmsg"));
	xmlNodeSetContent(point_msg, xmlStringf("%s: Point %s: %s", func, vtos(p), msg));
	xmlSetProp(point_msg, xmlString("level"), xmlStringf("%d", level));

	xmlNodePtr point = xmlNewNode(NULL, xmlString("point"));
	xmlNodeSetContent(point, xmlStringf("(%g %g %g", p.x, p.y, p.z));
	xmlAddChild(point_msg, point);

	Mon_SendXML(point_msg);

	Mon_Stdio(level, va("%s: Point %s: %s", func, vtos(p), msg));
}

/**
 * @brief Sends a winding to GtkRadiant.
 */
void Mon_SendWinding_(const char *func, mon_level_t level, const vec3_t p[], int32_t n, const char *msg) {

	xmlNodePtr winding_msg = xmlNewNode(NULL, xmlString("windingmsg"));
	xmlNodeSetContent(winding_msg, xmlStringf("%s: %s", func, msg));
	xmlSetProp(winding_msg, xmlString("level"), xmlStringf("%d", level));

	xmlNodePtr winding = xmlNewNode(NULL, xmlString("winding"));
	xmlNodeSetContent(winding, xmlStringf("%u ", n));
	xmlAddChild(winding_msg, winding);

	vec3_t center;
	center = Vec3_Zero();

	for (int32_t i = 0; i < n; i++) {
		xmlNodeAddContent(winding, xmlStringf("(%g %g %g)", p[i].x, p[i].y, p[i].z));
		center = Vec3_Add(center, p[i]);
	}

	Mon_SendXML(winding_msg);

	center = Vec3_Scale(center, 1.0 / n);
	Mon_Stdio(level, va("%s: Winding with center %s: %s", func, vtos(center), msg));
}

/**
 * @brief Connects to the specified host for XML process monitoring.
 */
_Bool Mon_Connect(const char *host) {

	Net_Init();

	if ((mon_state.socket = Net_Connect(host, NULL))) {
		Mem_InitBuffer(&mon_state.message, mon_state.buffer, sizeof(mon_state.buffer));

		Com_Print("@Connected to %s\n", host);

		mon_state.doc = xmlNewDoc(xmlString("1.0"));

		xmlNodePtr root = xmlNewNode(NULL, xmlString("q3map_feedback"));
		xmlSetProp(root, xmlString("version"), xmlString("1.0"));
		xmlDocSetRootElement(mon_state.doc, root);

		// because we stream child nodes on-demand, we must manually duplicate
		// the XML document header and root element
		Mon_SendString(xmlString("<?xml version=\"1.0\"?><q3map_feedback version=\"1\">"));

		// deliver the backlog of queued messages
		GList *backlog = mon_backlog;
		while (backlog) {
			Mon_SendXML((xmlNodePtr) backlog->data);
			backlog = backlog->next;
		}

		// and free the backlog
		g_list_free(mon_backlog);
		mon_backlog = NULL;

		return true;
	}

	return false;
}

/**
 * @brief Initialize BSP monitoring facilities (XML over TCP).
 */
void Mon_Init(void) {

	memset(&mon_state, 0, sizeof(mon_state));

	xmlInitParser();
}

/**
 * @brief Shuts down BSP monitoring facilities.
 */
void Mon_Shutdown(const char *msg) {

	if (mon_state.socket) {

		if (msg && *msg == '@') {
			// shut down immediately by an error in the monitor itself
		} else {
			if (msg) {
				Mon_SendMessage(MON_ERROR, msg);
			}
			Mon_SendString(xmlString("</q3map_feedback>"));
			SDL_Delay(1);
		}

		Net_CloseSocket(mon_state.socket);
		mon_state.socket = 0;
	}

	if (mon_state.doc) {
#if 0
		xmlSaveFile("/tmp/quemap_feedback.xml", mon_state.doc);
#endif

		xmlFreeDoc(mon_state.doc);
		mon_state.doc = NULL;

		g_list_free(mon_backlog);
	} else {
		g_list_free_full(mon_backlog, (GDestroyNotify) xmlFreeNode);
	}

	mon_backlog = NULL;

	xmlCleanupParser();

	Net_Shutdown();
}
