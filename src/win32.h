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

/*
* This file contains a minimal set of prototypes and defines
* necessary to achieve a win32 build using MinGW and MSYS.
*
* MinGW-3.1.0-1.exe, MSYS-1.0.10.exe, msysDTK-1.0.1.exe, and
* SDL-devel-1.2.11-mingw32.tar.gz are all required to build
* a functional client.
*
* After running ./configure, you must modify your Makefiles in order
* for gcc to find SDL and the winsock libraries.
*
* Specifically, add -I/path/to/sdl/include and -L/path/to/sdl/lib
* to CFLAGS.  Add -lSDLmain -lSDL -lwsock32 -lole32 to LDADD.
* Remove -ldl from LDADD, as it does not exist on Windows.
*
* To build the game module, remove -fPIC from CFLAGS.
*/

#ifdef _WIN32

#ifndef __WIN32_H__
#define __WIN32_H__

#include <windows.h>

#define SIGHUP 9999
#define SIGQUIT 9998

#define EWOULDBLOCK WSAEWOULDBLOCK
#define ECONNREFUSED WSAECONNREFUSED

#define RTLD_NOW 0
#define RTLD_LAZY 0

#undef PKGLIBDIR
#undef PKGDATADIR
#define PKGLIBDIR "."
#define PKGDATADIR "."

void *dlopen(const char *filename, int flag);
char *dlerror(void);
void *dlsym(void *handle, const char *symbol);
void dlclose(void *handle);

int ioctl(int sockfd, int flags, void *null);

#ifndef HAVE_STRCASESTR
char *strcasestr (char *haystack, char *needle);
#endif

#endif /* __WIN32_H__ */
#endif /* _WIN32 */
