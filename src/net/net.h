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

#include "net_types.h"

extern in_addr_t net_lo;

int32_t Net_GetError(void);
const char *Net_GetErrorString(void);

bool Net_CompareNetaddr(const net_addr_t *a, const net_addr_t *b);
bool Net_CompareClientNetaddr(const net_addr_t *a, const net_addr_t *b);

void Net_NetAddrToSockaddr(const net_addr_t *a, net_sockaddr *s);
const char *Net_NetaddrToString(const net_addr_t *a);
bool Net_StringToSockaddr(const char *s, net_sockaddr *saddr);
bool Net_StringToNetaddr(const char *s, net_addr_t *a);

int32_t Net_Socket(net_addr_type_t type, const char *iface, in_port_t port);
void Net_SetNonBlocking(int32_t sock, bool non_blocking);
void Net_CloseSocket(int32_t sock);

void Net_Init(void);
void Net_Shutdown(void);
