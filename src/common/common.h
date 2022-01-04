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

#pragma once

#include "shared/shared.h"

#include "atlas.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "filesystem.h"
#include "image.h"
#include "installer.h"
#include "mem.h"
#include "mem_buf.h"
#include "sys.h"
#include "thread.h"

/**
 * @brief The default game / cgame module name.
 */
#define DEFAULT_GAME		"default"

/**
 * @brief The default ai module name.
 */
#define DEFAULT_AI			"default"

/**
 * @brief Quake net protocol version; this must be changed when the structure
 * of core net messages or serialized data types change. The game and client
 * game maintain PROTOCOL_MINOR as well.
 */
#define PROTOCOL_MAJOR		1026

/**
 * @brief The IP address of the master server, where the authoritative list of
 * game servers is maintained.
 *
 * @see src/master/main.c
 */
#define HOST_MASTER 		"master.quetoo.org"

/**
 * @brief Default port for the master server.
 */
#define PORT_MASTER			1996

/**
 * @brief Default port for the client.
 */
#define PORT_CLIENT			1997

/**
 * @brief Default port for game server.
 */
#define PORT_SERVER			1998

/**
 * @brief Both the client and the server retain multiple snapshots of each
 * g_entity_t's state (entity_state_t) in order to calculate delta compression.
 */
#define PACKET_BACKUP		32
#define PACKET_MASK			(PACKET_BACKUP - 1)

/**
 * @brief The maximum number of entities to be referenced in a single message.
 */
#define MAX_PACKET_ENTITIES	128

/**
 * @brief Client bandwidth throttling thresholds, in bytes per second. Clients
 * may actually request that the server drops messages for them above a certain
 * bandwidth saturation point in order to maintain some level of connectivity.
 * However, they must accept at least 8KB/s.
 */
#define CLIENT_RATE_MIN		8192

/**
 * @brief Disallow dangerous downloads for both the client and server.
 */
#define IS_INVALID_DOWNLOAD(f) (!*f || *f == '/' || strstr(f, "..") || strchr(f, ' '))

/**
 * @brief Error categories.
 */
typedef enum {
	ERROR_DROP, // don't fully shit pants, but drop to console
	ERROR_FATAL, // program must exit
} err_t;

int32_t Com_Argc(void);
char *Com_Argv(int32_t arg);

void Com_PrintInfo(const char *s);

extern const char *DEBUG_CATEGORIES[DEBUG_TOTAL];

const char *Com_GetDebug(void);
void Com_SetDebug(const char *debug);

void Com_LogString(const char *str);

void Com_Debug_(const debug_t debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Com_Debugv_(const debug_t debug, const char *func, const char *fmt, va_list args) __attribute__((format(printf, 3, 0)));

void Com_Error_(err_t error, const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 3, 4)));
void Com_Errorv_(err_t error, const char *func, const char *fmt, va_list args) __attribute__((noreturn, format(printf, 3, 0)));

void Com_Print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Printv(const char *fmt, va_list args) __attribute__((format(printf, 1, 0)));

void Com_Verbose(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Verbosev(const char *fmt, va_list args) __attribute__((format(printf, 1, 0)));

void Com_Warn_(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Com_Warnv_(const char *func, const char *fmt, va_list args) __attribute__((format(printf, 2, 0)));

#define Com_Debug(debug, ...) Com_Debug_(debug, __func__, __VA_ARGS__)
#define Com_Debugv(debug, fmt, args) Com_Debugv_(debug, __func__, fmt, args)

#define Com_Error(error, ...) Com_Error_(error, __func__, __VA_ARGS__)
#define Com_Errorv(error, fmt, args) Com_Errorv_(error, __func__, fmt, args)

#define Com_Warn(...) Com_Warn_(__func__, __VA_ARGS__)
#define Com_Warnv(fmt, args) Com_Warnv_(__func__, fmt, args)

void Com_Init(int32_t argc, char *argv[]);
void Com_Shutdown(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

// subsystems
#define QUETOO_SERVER		0x1
#define QUETOO_GAME			0x2
#define QUETOO_CLIENT		0x4
#define QUETOO_CGAME		0x8
#define QUETOO_AI			0x10
#define QUEMAP				0x1000

/**
 * @brief Global engine structure.
 */
typedef struct {
	/**
	 * @brief The command line argument count.
	 */
	int32_t argc;

	/**
	 * @brief The command line arguments.
	 */
	char **argv;

	/**
	 * @brief Milliseconds since the application was started.
	 */
	uint32_t ticks;

	/**
	 * @brief The initialized subsystems.
	 */
	uint32_t subsystems;

	/**
	 * @brief The enabled debug categories.
	 */
	debug_t debug_mask;

	/**
	 * @brief Used by `Com_Error` to detect a cyclical error condition.
	 * @remarks If your Error function doesn't exit, make sure to set this to false.
	 */
	_Bool recursive_error;

	/**
	 * @brief Used by the common printing functions to spit out a file that we can
	 * use to diagnose startup errors.
	 */
	FILE *log_file;

	void (*Debug)(const debug_t debug, const char *msg);
	void (*Error)(err_t error, const char *msg) __attribute__((noreturn));
	void (*Print)(const char *msg);
	void (*Verbose)(const char *msg);
	void (*Warn)(const char *msg);

	void (*Init)(void);
	void (*Shutdown)(const char *msg);
} quetoo_t;

extern quetoo_t quetoo;

uint32_t Com_WasInit(uint32_t s);
void Com_InitSubsystem(uint32_t s);
void Com_QuitSubsystem(uint32_t s);

extern cvar_t *version;
extern cvar_t *revision;
extern cvar_t *dedicated;
extern cvar_t *game;
extern cvar_t *ai;
extern cvar_t *threads;
extern cvar_t *time_demo;
extern cvar_t *time_scale;
