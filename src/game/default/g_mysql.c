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

#include "g_local.h"

#if HAVE_MYSQL
#include <mysql.h>

typedef struct {
	MYSQL *mysql;
} g_mysql_state_t;

static g_mysql_state_t g_mysql_state;

/**
 * @brief Execute a MySQL query string.
 */
static void G_MySQL_Query(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void G_MySQL_Query(const char *fmt, ...) {
	char query[MAX_STRING_CHARS];
	va_list args;

	va_start(args, fmt);
	vsnprintf(query, sizeof(query), fmt, args);
	va_end(args);

	const int32_t error = mysql_query(g_mysql_state.mysql, query);
	if (error) {
		gi.Warn("%s -> %d", query, error);
	} else {
		gi.Debug("%s", query);
	}
}

/**
 * @return The MySQL-escaped name for the given entity.
 */
static const char *G_MySQL_EntityName(const g_entity_t *ent) {
	char name[MAX_NET_NAME];
	static char escaped[MAX_NET_NAME];

	if (!g_mysql_state.mysql) {
		return NULL;
	}

	if (!ent) {
		return "none";
	}

	if (!ent->client) {
		return ent->class_name;
	}

	StripColors(ent->client->locals.persistent.net_name, name);

	if (ent->client->ai) {
		g_strlcat(name, " [bot]", sizeof(name));
	}

	mysql_real_escape_string(g_mysql_state.mysql, name, escaped, sizeof(escaped));
	return escaped;
}

#endif

/**
 * @brief Records a frag to MySQL.
 */
void G_MySQL_ClientObituary(const g_entity_t *self, const g_entity_t *attacker, const uint32_t mod) {
#if HAVE_MYSQL

	if (!g_mysql_state.mysql) {
		return;
	}

	const char *fraggee = G_MySQL_EntityName(self);
	const char *fragger = G_MySQL_EntityName(attacker);

	G_MySQL_Query("INSERT INTO `frag` VALUES(NULL, NOW(), '%s', '%s', '%s', %d)",
	              g_level.name, fragger, fraggee, mod);
#endif
}

/**
 * @brief Initializes a connection MySQL (if available, and compiled).
 */
void G_MySQL_Init(void) {
#if HAVE_MYSQL

	memset(&g_mysql_state, 0, sizeof(g_mysql_state));

	const cvar_t *g_mysql = gi.AddCvar("g_mysql", "0", 0, NULL);
	if (g_mysql->value) {

		g_mysql_state.mysql = mysql_init(NULL);

		const char *host = gi.AddCvar("g_mysql_host", "localhost", 0, NULL)->string;
		const char *db = gi.AddCvar("g_mysql_db", "quetoo", 0, NULL)->string;
		const char *user = gi.AddCvar("g_mysql_user", "quetoo", 0, NULL)->string;
		const char *pass = gi.AddCvar("g_mysql_password", "", 0, NULL)->string;

		if (mysql_real_connect(g_mysql_state.mysql, host, user, pass, db, 0, NULL, 0)) {
			gi.Print("    MySQL: connected to %s/%s", host, db);
		} else {
			gi.Warn("Failed to connect to %s/%s: %s", host, db, mysql_error(g_mysql_state.mysql));
			G_MySQL_Shutdown();
		}
	}
#endif
}

/**
 * @brief Shutdown MySQL.
 */
void G_MySQL_Shutdown(void) {
#if HAVE_MYSQL

	if (g_mysql_state.mysql) {
		mysql_shutdown(g_mysql_state.mysql, SHUTDOWN_DEFAULT);
		g_mysql_state.mysql = NULL;
	}

#endif
}
