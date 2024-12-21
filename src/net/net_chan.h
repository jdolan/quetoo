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

#include "net_udp.h"
#include "net_message.h"

extern net_addr_t net_from;
extern mem_buf_t net_message;

void Netchan_Setup(net_src_t source, net_chan_t *chan, net_addr_t *addr, uint8_t qport);
void Netchan_Transmit(net_chan_t *chan, byte *data, size_t len);
void Netchan_OutOfBand(int32_t sock, const net_addr_t *addr, const void *data, size_t len);
void Netchan_OutOfBandPrint(int32_t sock, const net_addr_t *addr, const char *format, ...) __attribute__((format(printf,
        3, 4)));
bool Netchan_Process(net_chan_t *chan, mem_buf_t *msg);
void Netchan_Init(void);
void Netchan_Shutdown(void);
