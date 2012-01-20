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

#include "pak.h"

quake2world_t quake2world;

/*
 * main
 */
int main(int argc, char **argv) {
	char *pwd[] = { "." };
	err = ERR_OK;

	if (argc == 1 || !strcmp(argv[1], "--help")) { // print help
		printf("Quake2World `pak' manipulates Quake2(tm) pak files.\n\n"
			"Examples:\n"
			"  pak -c file.pak foo bar	#create file.pak from foo and bar\n"
			"  pak -x file.pak dest		#extract file.pak to dest\n"
			"  pak -t file.pak		#list the contents of file.pak\n");
		exit(0);
	}

	if (argc < 3) { // bad options
		fprintf(stderr, "%s: You must specify one of the options `-tcx'.\n"
			"Try `%s --help' for more information.\n", argv[0], argv[0]);
		exit(ERR_ARG);
	}

	memset(&quake2world, 0, sizeof(quake2world));

	Z_Init();

	if (!strcmp(argv[1], "-c")) { // create
		if (argc - 3 == 0) // special case when no dirs specified
			Pak_CreatePakfile(argv[2], 1, pwd);
		else
			Pak_CreatePakfile(argv[2], argc - 3, argv + 3);
	} else if (!strcmp(argv[1], "-x")) { // extract
		Pak_ExtractPakfile(argv[2], argc > 3 ? argv[3] : NULL, false);
	} else if (!strcmp(argv[1], "-t")) { // test
		Pak_ExtractPakfile(argv[2], argc > 3 ? argv[3] : NULL, true);
	} else {
		fprintf(stderr, "%s: Missing or unsupported option.\n"
			"Try `%s --help' for more information.\n", argv[0], argv[0]);
		exit(ERR_ARG);
	}

	return err;
}
