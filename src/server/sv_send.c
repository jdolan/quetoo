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

#include "server.h"

/*
 *
 * Com_Printf redirection
 *
 */

char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void Sv_FlushRedirect(int target, char *outputbuf){

	switch(target){
		case RD_PACKET:
			Netchan_OutOfBandPrint(NS_SERVER, net_from, "print\n%s", outputbuf);
			break;
		case RD_CLIENT:
			Msg_WriteByte(&sv_client->netchan.message, svc_print);
			Msg_WriteByte(&sv_client->netchan.message, PRINT_HIGH);
			Msg_WriteString(&sv_client->netchan.message, outputbuf);
			break;
		default:
			Com_Debug("Sv_FlushRedirect: %d\n", target);
			break;
	}
}


/*
 *
 * EVENT MESSAGES
 *
 */


/*
 * Sv_ClientPrint
 *
 * Sends text across to be displayed if the level filter passes.
 */
void Sv_ClientPrint(edict_t *ent, int level, const char *fmt, ...){
	sv_client_t *cl;
	va_list	argptr;
	char string[MAX_STRING_CHARS];
	int n;

	n = NUM_FOR_EDICT(ent);
	if(n < 1 || n > sv_maxclients->value){
		Com_Warn("Sv_ClientPrint: Issued to non-client.\n");
		return;
	}

	cl = &svs.clients[n - 1];

	if(cl->state != cs_spawned){
		Com_Debug("Sv_ClientPrint: Issued to unspawned client.\n");
		return;
	}

	if(level < cl->messagelevel){
		Com_Debug("Sv_ClientPrint: Filtered by message level.\n");
		return;
	}

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	Msg_WriteByte(&cl->netchan.message, svc_print);
	Msg_WriteByte(&cl->netchan.message, level);
	Msg_WriteString(&cl->netchan.message, string);
}


/*
 * Sv_ClientCenterPrint
 *
 * Center-print to a single client.  This is sent via Sv_Unicast so that it
 * is transmitted over the reliable channel; center-prints are important.
 */
void Sv_ClientCenterPrint(edict_t *ent, const char *fmt, ...){
	char msg[1024];
	va_list	argptr;
	int n;

	n = NUM_FOR_EDICT(ent);
	if(n < 1 || n > sv_maxclients->value){
		Com_Warn("Sv_ClientCenterPrint: ClientCenterPrintf to non-client.\n");
		return;
	}

	va_start(argptr, fmt);
	vsprintf(msg, fmt, argptr);
	va_end(argptr);

	Msg_WriteByte(&sv.multicast, svc_centerprint);
	Msg_WriteString(&sv.multicast, msg);

	Sv_Unicast(ent, true);
}


/*
 * Sv_BroadcastPrint
 *
 * Sends text to all active clients over their unreliable channels.
 */
void Sv_BroadcastPrint(int level, const char *fmt, ...){
	va_list	argptr;
	char string[MAX_STRING_CHARS];
	sv_client_t *cl;
	int i;

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	// echo to console
	if(dedicated->value){
		char copy[MAX_STRING_CHARS];
		int j;

		// mask off high bits
		for(j = 0; j < MAX_STRING_CHARS - 1 && string[j]; j++)
			copy[j] = string[j] & 127;
		copy[j] = 0;
		Com_Print("%s", copy);
	}

	for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++){

		if(level < cl->messagelevel)
			continue;

		if(cl->state != cs_spawned)
			continue;

		Msg_WriteByte(&cl->netchan.message, svc_print);
		Msg_WriteByte(&cl->netchan.message, level);
		Msg_WriteString(&cl->netchan.message, string);
	}
}


/*
 * Sv_BroadcastCommand
 *
 * Sends text to all active clients
 */
void Sv_BroadcastCommand(const char *fmt, ...){
	va_list	argptr;
	char string[MAX_STRING_CHARS];

	if(!sv.state)
		return;
	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	Msg_WriteByte(&sv.multicast, svc_stufftext);
	Msg_WriteString(&sv.multicast, string);
	Sv_Multicast(NULL, MULTICAST_ALL_R);
}


/*
 * Sv_Unicast
 *
 * Sends the contents of the mutlicast buffer to a single client
 */
void Sv_Unicast(edict_t *ent, qboolean reliable){
	int n;
	sv_client_t *cl;

	if(!ent)
		return;

	n = NUM_FOR_EDICT(ent);
	if(n < 1 || n > sv_maxclients->value)
		return;

	cl = svs.clients + (n - 1);

	if(reliable)
		Sb_Write(&cl->netchan.message, sv.multicast.data, sv.multicast.cursize);
	else
		Sb_Write(&cl->datagram, sv.multicast.data, sv.multicast.cursize);

	Sb_Clear(&sv.multicast);
}


/*
 * Sv_Multicast
 *
 * Sends the contents of sv.multicast to a subset of the clients,
 * then clears sv.multicast.
 *
 * MULTICAST_ALL	same as broadcast (origin can be NULL)
 * MULTICAST_PVS	send to clients potentially visible from org
 * MULTICAST_PHS	send to clients potentially hearable from org
 */
void Sv_Multicast(vec3_t origin, multicast_t to){
	sv_client_t *client;
	byte *mask;
	int leafnum, cluster;
	int j;
	qboolean reliable;
	int area1, area2;

	reliable = false;

	if(to != MULTICAST_ALL_R && to != MULTICAST_ALL){
		leafnum = Cm_PointLeafnum(origin);
		area1 = Cm_LeafArea(leafnum);
	} else {
		leafnum = 0;  // just to avoid compiler warnings
		area1 = 0;
	}

	switch(to){
		case MULTICAST_ALL_R:
			reliable = true;  // intentional fallthrough
		case MULTICAST_ALL:
			leafnum = 0;
			mask = NULL;
			break;

		case MULTICAST_PHS_R:
			reliable = true;  // intentional fallthrough
		case MULTICAST_PHS:
			leafnum = Cm_PointLeafnum(origin);
			cluster = Cm_LeafCluster(leafnum);
			mask = Cm_ClusterPHS(cluster);
			break;

		case MULTICAST_PVS_R:
			reliable = true;  // intentional fallthrough
		case MULTICAST_PVS:
			leafnum = Cm_PointLeafnum(origin);
			cluster = Cm_LeafCluster(leafnum);
			mask = Cm_ClusterPVS(cluster);
			break;

		default:
			Com_Warn("Sv_Multicast: bad multicast: %i.\n", to);
			return;
	}

	// send the data to all relevent clients
	for(j = 0, client = svs.clients; j < sv_maxclients->value; j++, client++){

		if(client->state == cs_free)
			continue;

		if(client->state != cs_spawned && !reliable)
			continue;

		if(mask){
			leafnum = Cm_PointLeafnum(client->edict->s.origin);
			cluster = Cm_LeafCluster(leafnum);
			area2 = Cm_LeafArea(leafnum);
			if(!Cm_AreasConnected(area1, area2))
				continue;
			if(mask && (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
				continue;
		}

		if(reliable)
			Sb_Write(&client->netchan.message, sv.multicast.data, sv.multicast.cursize);
		else
			Sb_Write(&client->datagram, sv.multicast.data, sv.multicast.cursize);
	}

	Sb_Clear(&sv.multicast);
}


/*
 * Sv_StartSound
 *
 * FIXME: if entity isn't in PHS, they must be forced to be sent or
 * have the origin explicitly sent.
 *
 * An attenuation of 0 will play full volume everywhere in the level.
 * Larger attenuation will drop off (max 4 attenuation).
 *
 * If origin is NULL, the origin is determined from the entity origin
 * or the midpoint of the entity box for bmodels.
 */
void Sv_PositionedSound(vec3_t origin, edict_t *entity, int soundindex, int atten){
	int flags;
	int i;
	vec3_t org;

	if(atten < ATTN_NONE || atten > ATTN_STATIC){
		Com_Warn("Sv_StartSound: attenuation %d.\n", atten);
		atten = DEFAULT_SOUND_ATTENUATION;
	}

	flags = 0;
	if(atten != DEFAULT_SOUND_ATTENUATION)
		flags |= S_ATTEN;

	// the client doesn't know that bsp models have weird origins
	// the origin can also be explicitly set
	if((entity->svflags & SVF_NOCLIENT) || (entity->solid == SOLID_BSP) || origin)
		flags |= S_ORIGIN;
	else
		flags |= S_ENTNUM;

	// use the entity origin unless it is a bsp model or explicitly specified
	if(origin)
		VectorCopy(origin, org);
	else {
		if(entity->solid == SOLID_BSP){
			for(i = 0; i < 3; i++)
				org[i] = entity->s.origin[i] + 0.5 * (entity->mins[i] + entity->maxs[i]);
		} else {
			VectorCopy(entity->s.origin, org);
		}
	}

	Msg_WriteByte(&sv.multicast, svc_sound);
	Msg_WriteByte(&sv.multicast, flags);
	Msg_WriteByte(&sv.multicast, soundindex);

	if(flags & S_ATTEN)
		Msg_WriteByte(&sv.multicast, atten);

	if(flags & S_ENTNUM)
		Msg_WriteShort(&sv.multicast, NUM_FOR_EDICT(entity));

	if(flags & S_ORIGIN)
		Msg_WritePos(&sv.multicast, org);

	if(atten != ATTN_NONE)
		Sv_Multicast(org, MULTICAST_PHS);
	else
		Sv_Multicast(org, MULTICAST_ALL);
}


/*
 *
 * FRAME UPDATES
 *
 */

int zlib_accum = 0;  // count of bytes saved via zlib

#include <zlib.h>
static z_stream z;
static byte zbuf[MAX_MSGLEN];

/*
 * Sv_ZlibClientDatagrab
 *
 * Deflates msg for clients supporting svc_zlib, and rewrites it if
 * the compression resulted in a smaller packet.
 */
static void Sv_ZlibClientDatagram(sv_client_t *client, sizebuf_t *msg){
	int len;

	if(!((int)sv_extensions->value & QUAKE2WORLD_ZLIB))  // some servers may elect not to use this
		return;

	if(!((int)client->extensions & QUAKE2WORLD_ZLIB))  // some clients wont support it
		return;

	if(msg->cursize < 600)  // and some payloads are too small to bother
		return;

	memset(zbuf, 0, MAX_MSGLEN);

	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	deflateInit2(&z, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);

	z.avail_in = msg->cursize;
	z.next_in = msg->data;

	z.avail_out = MAX_MSGLEN;
	z.next_out = zbuf;

	deflate(&z, Z_FULL_FLUSH);
	len = MAX_MSGLEN - z.avail_out;

	deflateEnd(&z);

	if(len >= msg->cursize)  // compression not beneficial
		return;

	zlib_accum += (msg->cursize - len);
	memset(msg->data, 0, MAX_MSGLEN);
	msg->data[0] = svc_zlib;
	memcpy(msg->data + 1, zbuf, len);
	msg->cursize = len + 1;
}


/*
 * Sv_SendClientDatagram
 */
static qboolean Sv_SendClientDatagram(sv_client_t *client){
	byte msg_buf[MAX_MSGLEN];
	sizebuf_t msg;

	Sv_BuildClientFrame(client);

	Sb_Init(&msg, msg_buf, sizeof(msg_buf));
	msg.allowoverflow = true;

	// send over all the relevant entity_state_t
	// and the player_state_t
	Sv_WriteFrameToClient(client, &msg);

	// copy the accumulated multicast datagram
	// for this client out to the message
	// it is necessary for this to be after the WriteEntities
	// so that entity references will be current
	if(client->datagram.overflowed)
		Com_Warn("Datagram overflowed for %s.\n", client->name);
	else
		Sb_Write(&msg, client->datagram.data, client->datagram.cursize);
	Sb_Clear(&client->datagram);

	if(msg.overflowed){  // must have room left for the packet header
		Com_Warn("Message overflowed for %s.\n", client->name);
		Sb_Clear(&msg);
	}

	Sv_ZlibClientDatagram(client, &msg);  // if zlib is available, use it

	// send the datagram
	Netchan_Transmit(&client->netchan, msg.cursize, msg.data);

	// record the size for rate estimation
	client->message_size[sv.framenum % CLIENT_RATE_MESSAGES] = msg.cursize;

	return true;
}


/*
 * Sv_DemoCompleted
 */
static void Sv_DemoCompleted(void){
	Sv_Shutdown("Demo complete.\n");
}


/*
 * Sv_RateDrop
 *
 * Returns true if the client is over its current
 * bandwidth estimation and should not be sent another packet
 */
static qboolean Sv_RateDrop(sv_client_t *c){
	int total;
	int i;

	// never drop over the loopback
	if(c->netchan.remote_address.type == NA_LOCAL)
		return false;

	total = 0;

	for(i = 0; i < CLIENT_RATE_MESSAGES; i++){
		total += c->message_size[i];
	}

	if(total > c->rate){
		c->surpress_count++;
		c->message_size[sv.framenum % CLIENT_RATE_MESSAGES] = 0;
		return true;
	}

	return false;
}


/*
 * Sv_SendClientMessages
 */
void Sv_SendClientMessages(void){
	int i;
	sv_client_t *c;
	int msglen;
	byte msgbuf[MAX_MSGLEN];
	int r;

	if(!svs.initialized)
		return;

	msglen = 0;

	// read the next demo message if needed
	if(sv.state == ss_demo && sv.demofile){

		r = Fs_Read(&msglen, 4, 1, sv.demofile);

		if(r != 1){  // improperly terminated demo file
			Com_Warn("Sv_SendClientMessages: Failed to read msglen from demo file.\n");
			Sv_DemoCompleted();
			return;
		}

		msglen = LittleLong(msglen);

		if(msglen == -1){  // properly terminated demo file
			Sv_DemoCompleted();
			return;
		}

		if(msglen > MAX_MSGLEN){  // corrupt demo file
			Com_Warn("Sv_SendClientMessages: %d > MAX_MSGLEN.\n", msglen);
			return;
		}

		r = Fs_Read(msgbuf, msglen, 1, sv.demofile);

		if(r != 1){
			Com_Warn("Sv_SendClientMessages: Incomplete or corrupt demo file.\n");
			Sv_DemoCompleted();
			return;
		}
	}

	// send a message to each connected client
	for(i = 0, c = svs.clients; i < sv_maxclients->value; i++, c++){

		if(!c->state)  // don't bother
			continue;

		if(c->netchan.message.overflowed){  // drop the client
			Sb_Clear(&c->netchan.message);
			Sb_Clear(&c->datagram);
			Sv_BroadcastPrint(PRINT_HIGH, "%s overflowed\n", c->name);
			Sv_DropClient(c);
		}

		if(sv.state == ss_demo){  // send the demo packet
			Netchan_Transmit(&c->netchan, msglen, msgbuf);
		}
		else if(c->state == cs_spawned){  // send the game packet

			if(Sv_RateDrop(c))  // don't overrun bandwidth
				continue;

			Sv_SendClientDatagram(c);
		}
		else {  // just update reliable if needed
			if(c->netchan.message.cursize ||
					quake2world.time - c->netchan.last_sent > 1000)
				Netchan_Transmit(&c->netchan, 0, NULL);
		}
	}
}

