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

// we have to stub a few things from common and shared
// since Pak_ReadPakfile uses them, and we need it

void *Z_Malloc(size_t size){
	return malloc(size);
}

void Z_Free(void *ptr){
	free(ptr);
}

void Com_Warn(const char *fmt, ...){
	fprintf(stderr, "%s", fmt);
}

int LittleLong(int l){

	static union {
		byte b[2];
		unsigned short s;
	} swaptest;

	swaptest.b[0] = 1;
	swaptest.b[1] = 0;

	byte b1, b2, b3, b4;

	if(swaptest.s == 1)
		return l;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}


/*
 * main
 */
int main(int argc, char **argv){
	char *pwd[] = {"."};
	err = ERR_OK;

	if(argc == 1 || !strcmp(argv[1], "--help")){  // print help
		printf("Quake2World `pak' manipulates Quake2(tm) pak files.\n\n"
				"Examples:\n"
				"  pak -c file.pak foo bar	#create file.pak from foo and bar\n"
				"  pak -x file.pak dest		#extract file.pak to dest\n"
				"  pak -t file.pak		#list the contents of file.pak\n"
		);
		exit(0);
	}

   	if(argc < 3){  // bad options
		fprintf(stderr, "%s: You must specify one of the options `-tcx'.\n"
				"Try `%s --help' for more information.\n", argv[0], argv[0]);
		exit(ERR_ARG);
	}

	if(!strcmp(argv[1], "-c")){  // create
		if(argc - 3 == 0)  // special case when no dirs specified
			Pak_CreatePakfile(argv[2], 1, pwd);
		else
			Pak_CreatePakfile(argv[2], argc - 3, argv + 3);
	}
	else if(!strcmp(argv[1], "-x")){  // extract
		Pak_ExtractPakfile(argv[2], argc > 3 ? argv[3] : NULL, false);
	}
	else if(!strcmp(argv[1], "-t")){  // test
		Pak_ExtractPakfile(argv[2], argc > 3 ? argv[3] : NULL, true);
	}
	else {
		fprintf(stderr, "%s: Missing or unsupported option.\n"
				"Try `%s --help' for more information.\n", argv[0], argv[0]);
		exit(ERR_ARG);
	}

	return err;
}
