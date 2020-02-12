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

#include "cl_local.h"

#define NET_GRAPH_WIDTH 256
#define NET_GRAPH_Y 0

// net graph samples
typedef struct {
	float value;
	color_t color;
} net_graph_sample_t;

static net_graph_sample_t net_graph_samples[NET_GRAPH_WIDTH];
static int32_t num_net_graph_samples;

/**
 * @brief Accumulates a net graph sample.
 */
static void Cl_NetGraph(float value, const color_t color) {

	net_graph_samples[num_net_graph_samples].value = value;
	net_graph_samples[num_net_graph_samples].color = color;

	if (net_graph_samples[num_net_graph_samples].value > 1.0) {
		net_graph_samples[num_net_graph_samples].value = 1.0;
	}

	num_net_graph_samples++;

	if (num_net_graph_samples == NET_GRAPH_WIDTH) {
		num_net_graph_samples = 0;
	}
}

/**
 * @brief Accumulates net graph samples for the current frame. Dropped or
 * suppressed packets are recorded as peak samples, and packet latency is
 * recorded over a range of 0-300ms.
 */
void Cl_AddNetGraph(void) {
	uint32_t i;

	// we only need to do our accounting when asked to
	if (!cl_draw_net_graph->value) {
		return;
	}

	for (i = 0; i < cls.net_chan.dropped; i++) {
		Cl_NetGraph(1.0, color_red);
	}

	for (i = 0; i < cl.suppress_count; i++) {
		Cl_NetGraph(1.0, color_yellow);
	}

	// see what the latency was on this packet
	const uint32_t frame = cls.net_chan.incoming_acknowledged & CMD_MASK;
	const uint32_t ping = cl.unclamped_time - cl.cmds[frame].timestamp;

	Cl_NetGraph(ping / 300.0, color_green); // 300ms is lagged out
}

/**
 * @brief Provides a real-time visual representation of latency and packet loss
 * via a small graph drawn on screen.
 */
static void Cl_DrawNetGraph(void) {
	int32_t i, j, x, y, h;

	if (!cl_draw_net_graph->value) {
		return;
	}

	r_pixel_t ch;
	R_BindFont("small", NULL, &ch);

	const r_pixel_t netgraph_height = ch * 3;

	x = r_context.width - NET_GRAPH_WIDTH;
	y = r_context.height - NET_GRAPH_Y - netgraph_height;

	R_DrawFill(x, y, NET_GRAPH_WIDTH, netgraph_height, color4bv(0x80808080));

	for (i = 0; i < NET_GRAPH_WIDTH; i++) {

		j = (num_net_graph_samples - i) & (NET_GRAPH_WIDTH - 1);
		h = net_graph_samples[j].value * netgraph_height;

		if (!h) {
			continue;
		}

		x = r_context.width - i;
		y = r_context.height - NET_GRAPH_Y - h;

		R_DrawFill(x, y, 1, h, net_graph_samples[j].color);
	}
}

/**
 * @brief Draws counters and performance information about the renderer.
 */
static void Cl_DrawRendererStats(void) {
	r_pixel_t ch, y = 64;

	if (!cl_draw_renderer_stats->value) {
		return;
	}

	if (cls.state != CL_ACTIVE) {
		return;
	}

	R_BindFont("small", NULL, &ch);
	R_DrawString(0, y, "BSP:", color_yellow);
	y += ch;

	R_DrawString(0, y, va("%d nodes", r_view.count_bsp_nodes), color_yellow);
	y += ch;

	R_DrawString(0, y, va("%d draw elements", r_view.count_bsp_draw_elements), color_yellow);
	y += ch;

	y += ch;
	R_DrawString(0, y, "Mesh:", color_cyan);
	y += ch;

	R_DrawString(0, y, va("%d models", r_view.count_mesh_models), color_cyan);
	y += ch;

	R_DrawString(0, y, va("%d triangles", r_view.count_mesh_triangles), color_cyan);
	y += ch;

	y += ch;
	R_DrawString(0, y, "2D:", color_blue);
	y += ch;

	R_DrawString(0, y, va("%d arrays", r_view.count_draw_arrays), color_blue);
	y += ch;

	y += ch;
	R_DrawString(0, y, "Other:", color_white);
	y += ch;

	R_DrawString(0, y, va("%d lights", r_view.num_lights), color_white);
	y += ch;

	R_DrawString(0, y, va("%d particles", r_view.num_particles), color_white);
	y += ch;

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws counters and performance information about the sound subsystem.
 */
static void Cl_DrawSoundStats(void) {
	r_pixel_t ch, y = cl_draw_renderer_stats->value ? 400 : 64;

	if (!cl_draw_sound_stats->value) {
		return;
	}

	if (cls.state != CL_ACTIVE) {
		return;
	}

	R_BindFont("small", NULL, &ch);

	R_DrawString(0, y, "Sound:", color_magenta);
	y += ch;

	R_DrawString(0, y, va("%d channels", s_env.num_active_channels), color_magenta);
	y += ch;

	for (int32_t i = 0; i < MAX_CHANNELS; i++) {
		const s_channel_t *channel = &s_env.channels[i];

		if (!channel->sample)
			continue;

		ALenum state;
		alGetSourcei(s_env.sources[i], AL_SOURCE_STATE, &state);
		S_CheckALError();

		if (state != AL_PLAYING)
			continue;

		R_DrawString(ch, y, va("%i: %s", i, channel->sample->media.name), color_magenta);
		y += ch;
	}

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cl_DrawCounters(void) {
	static vec3_t velocity;
	static char pps[8], fps[8], spd[8];
	static int32_t last_draw_time, last_speed_time;
	r_pixel_t cw, ch;

	if (!cl_draw_counters->integer) {
		return;
	}

	R_BindFont("small", &cw, &ch);

	const r_pixel_t x = r_context.width - 7 * cw;
	r_pixel_t y = r_context.height - 3 * ch;

	cl.frame_counter++;

	if (quetoo.ticks - last_speed_time >= 100) {

		velocity = cl.frame.ps.pm_state.velocity;
		velocity.z = 0.0;

		g_snprintf(spd, sizeof(spd), "%4.0fspd", vec3_length(velocity));

		last_speed_time = quetoo.ticks;
	}

	if (quetoo.ticks - last_draw_time >= 1000) {

		g_snprintf(fps, sizeof(fps), "%4ufps", cl.frame_counter);
		g_snprintf(pps, sizeof(pps), "%4upps", cl.packet_counter);

		last_draw_time = quetoo.ticks;

		cl.frame_counter = 0;
		cl.packet_counter = 0;
	}

	if (cl_draw_position->integer) {
		R_DrawString(r_context.width - 14 * cw, y - ch, va("%4.0f %4.0f %4.0f",
														   cl.frame.ps.pm_state.origin.x,
														   cl.frame.ps.pm_state.origin.y,
														   cl.frame.ps.pm_state.origin.z), color_white);
	}

	R_DrawString(x, y, spd, color_white);
	y += ch;

	R_DrawString(x, y, fps, color_white);
	y += ch;

	R_DrawString(x, y, pps, color_white);
	y += ch;

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */

/**
 * @brief This is called every frame, and can also be called explicitly to flush
 * text to the screen.
 */
void Cl_UpdateScreen(void) {

	R_BeginFrame();

	if (cls.state == CL_ACTIVE) {

		R_DrawView(&r_view);

		Cl_DrawChat();
		Cl_DrawNetGraph();
		Cl_DrawCounters();

		if (cls.key_state.dest != KEY_CONSOLE && cls.key_state.dest != KEY_UI) {
			Cl_DrawNotify();
			Cl_DrawRendererStats();
			Cl_DrawSoundStats();

			cls.cgame->UpdateScreen(&cl.frame);
		}
	}

	if (cls.state != CL_LOADING && cls.key_state.dest == KEY_CONSOLE) {
		Cl_DrawConsole();
	}

	if (cls.key_state.dest == KEY_UI || cls.state == CL_LOADING) {
		Ui_Draw();
	}

	R_Draw2D();

	R_EndFrame();

	Cl_ClearView();
}
