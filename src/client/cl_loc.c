/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "cl_local.h"

typedef struct loc_s {
	vec3_t loc;
	char desc[MAX_STRING_CHARS];
} loc_t;

#define MAX_LOCATIONS 1024

static loc_t locations[MAX_LOCATIONS];
static int num_locations;

/*
 * Cl_ClearLocations
 *
 * Effectively clears all locations for the current level.
 */
static void Cl_ClearLocations(void) {
	num_locations = 0;
}

/*
 * Cl_LoadLocations
 *
 * Parse a .loc file for the current level.
 */
void Cl_LoadLocations(void) {
	const char *c;
	char file_name[MAX_QPATH];
	FILE *f;
	int i;

	Cl_ClearLocations(); // clear any resident locations
	i = 0;

	// load the locations file
	c = Basename(cl.config_strings[CS_MODELS + 1]);
	snprintf(file_name, sizeof(file_name), "locations/%s", c);
	strcpy(file_name + strlen(file_name) - 3, "loc");

	if (Fs_OpenFile(file_name, &f, FILE_READ) == -1) {
		Com_Debug("Couldn't load %s\n", file_name);
		return;
	}

	while (i < MAX_LOCATIONS) {

		const int err = fscanf(f, "%f %f %f %[^\n]", &locations[i].loc[0],
				&locations[i].loc[1], &locations[i].loc[2], locations[i].desc);

		num_locations = i;
		if (err == EOF)
			break;
		i++;
	}

	Cl_LoadProgress(100);

	Com_Print("Loaded %i locations.\n", num_locations);
	Fs_CloseFile(f);
}

/*
 * Cl_SaveLocations_f
 *
 * Write locations for current level to file.
 */
static void Cl_SaveLocations_f(void) {
	char file_name[MAX_QPATH];
	FILE *f;
	int i;

	snprintf(file_name, sizeof(file_name), "%s/%s", Fs_Gamedir(), cl.config_strings[CS_MODELS + 1]);
	strcpy(file_name + strlen(file_name) - 3, "loc"); // change to .loc

	if ((f = fopen(file_name, "w")) == NULL) {
		Com_Warn("Cl_SaveLocations_f: Failed to write %s\n", file_name);
		return;
	}

	for (i = 0; i < num_locations; i++) {
		fprintf(f, "%d %d %d %s\n", (int) locations[i].loc[0],
				(int) locations[i].loc[1], (int) locations[i].loc[2],
				locations[i].desc);
	}

	Com_Print("Saved %d locations.\n", num_locations);
	Fs_CloseFile(f);
}

/*
 * Cl_Location
 *
 * Returns the description of the location nearest nearto.
 */
static const char *Cl_Location(const vec3_t nearto) {
	vec_t dist, mindist;
	vec3_t v;
	int i, j;

	if (num_locations == 0)
		return "";

	mindist = 999999;

	for (i = 0, j = 0; i < num_locations; i++) { // find closest loc

		VectorSubtract(nearto, locations[i].loc, v);
		if ((dist = VectorLength(v)) < mindist) { // closest yet
			mindist = dist;
			j = i;
		}
	}

	return locations[j].desc;
}

/*
 * Cl_LocationHere
 *
 * Returns the description of the location nearest the client.
 */
const char *Cl_LocationHere(void) {
	return Cl_Location(r_view.origin);
}

/*
 * Cl_LocationThere
 *
 * Returns the description of the location nearest the client's crosshair.
 */
const char *Cl_LocationThere(void) {
	vec3_t dest;

	// project vector from view position and angle
	VectorMA(r_view.origin, 8192.0, r_view.forward, dest);

	// and trace to world model
	R_Trace(r_view.origin, dest, 0, MASK_SHOT);

	return Cl_Location(r_view.trace.end);
}

/*
 * Cl_AddLocation
 *
 * Add a new location described by desc at nearto.
 */
static void Cl_AddLocation(const vec3_t nearto, const char *desc) {

	if (num_locations >= MAX_LOCATIONS)
		return;

	VectorCopy(nearto, locations[num_locations].loc);
	strncpy(locations[num_locations].desc, desc, MAX_STRING_CHARS);

	num_locations++;
}

/*
 * Cl_AddLocation_f
 *
 * Command callback for adding locations in game.
 */
static void Cl_AddLocation_f(void) {

	if (Cmd_Argc() < 2) {
		Com_Print("Usage: %s <description>\n", Cmd_Argv(0));
		return;
	}

	Cl_AddLocation(r_view.origin, Cmd_Args());
}

/*
 * Cl_InitLocations
 */
void Cl_InitLocations(void) {
	Cmd_AddCommand("addloc", Cl_AddLocation_f, NULL);
	Cmd_AddCommand("savelocs", Cl_SaveLocations_f, NULL);
}

/*
 * Cl_ShutdownLocations
 */
void Cl_ShutdownLocations(void) {
	Cmd_RemoveCommand("addloc");
	Cmd_RemoveCommand("savelocs");
}
