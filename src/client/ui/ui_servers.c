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

#include "ui_local.h"

/*
 *
 */
static TW_CALL void Ui_Servers_Connect(void *data){
	cl_server_info_t *s = (cl_server_info_t *)data;

	strcpy(cls.server_name, Net_NetaddrToString(s->addr));
}

/*
 *
 */
static TW_CALL void Ui_Servers_Refresh(void *data){
	TwBar *bar = (TwBar *)data;
	cl_server_info_t *s = cls.servers;

	Cl_Servers_f();

	while(s){
		TwAddButton(bar, s->info, Ui_Servers_Connect, s, NULL);
		s = s->next;
	}
}


/*
 * Ui_Servers
 */
TwBar *Ui_Servers(void){

	TwBar *bar = TwNewBar("Servers");

	TwAddButton(bar, "Refresh", Ui_Servers_Refresh, bar, NULL);

	TwDefine("Servers size='600 400' iconified=true");

	return bar;
}
