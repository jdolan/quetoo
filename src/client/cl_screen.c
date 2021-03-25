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

#define NET_GRAPH_WIDTH 128
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
	int32_t x, y;

	if (!cl_draw_net_graph->value) {
		return;
	}

	r_pixel_t ch;
	R_BindFont("small", NULL, &ch);

	const r_pixel_t netgraph_height = ch * 3;

	x = r_context.width - NET_GRAPH_WIDTH;
	y = r_context.height - NET_GRAPH_Y - netgraph_height;

	R_Draw2DFill(x, y, NET_GRAPH_WIDTH, netgraph_height, Color4bv(0x80404040));

	for (int32_t i = 0; i < NET_GRAPH_WIDTH; i++) {

		const int32_t j = (num_net_graph_samples - i) & (NET_GRAPH_WIDTH - 1);
		const int32_t h = net_graph_samples[j].value * netgraph_height;

		if (!h) {
			continue;
		}

		x = r_context.width - i;
		y = r_context.height - NET_GRAPH_Y;

		const r_pixel_t points[4] = { x, y, x, y - h };
		R_Draw2DLines(points, 2, net_graph_samples[j].color);
	}
}

/**
 * @brief Draws counters and performance information about the renderer.
 */
static void Cl_DrawRendererStats(void) {
	r_pixel_t ch, x = 1, y = 64;

	if (!cl_draw_renderer_stats->value) {
		return;
	}

	if (cls.state != CL_ACTIVE) {
		return;
	}

	if (cls.key_state.dest != KEY_GAME) {
		y = Cl_GetConsoleHeight();
	}

	R_BindFont("small", NULL, &ch);

	{
		R_Draw2DString(x, y, "BSP:", color_yellow);
		y += ch;
		R_Draw2DString(x, y, va(" %d inline models", r_stats.count_bsp_inline_models), color_yellow);
		y += ch;
		R_Draw2DString(x, y, va(" %d draw elements", r_stats.count_bsp_draw_elements), color_yellow);
		y += ch;
		R_Draw2DString(x, y, va(" %d blend elements", r_stats.count_bsp_draw_elements_blend), color_yellow);
		y += ch;
		R_Draw2DString(x, y, va(" %d triangles", r_stats.count_bsp_triangles), color_yellow);
		y += ch;
		R_Draw2DString(x, y, va(" %d occlusion queries (%d passed)", r_stats.count_bsp_occlusion_queries,
								r_stats.count_bsp_occlusion_queries_passed), color_yellow);
		y += ch;
	}

	y += ch;

	{
		R_Draw2DString(x, y, "Mesh:", color_yellow);
		y += ch;
		R_Draw2DString(x, y, va(" %d models", r_stats.count_mesh_models), color_yellow);
		y += ch;
		R_Draw2DString(x, y, va(" %d triangles", r_stats.count_mesh_triangles), color_yellow);
		y += ch;
	}

	y += ch;

	{
		R_Draw2DString(x, y, "Sprites:", color_yellow);
		y += ch;

		static char sprites[64], beams[64], instances[64], draw_elements[64];
		static uint32_t sprite_time;

		if (quetoo.ticks - sprite_time > 100) {
			sprite_time = quetoo.ticks;

			g_snprintf(sprites, sizeof(sprites),      " %d sprites", cl_view.num_sprites);
			g_snprintf(beams, sizeof(beams),          " %d beams", cl_view.num_beams);
			g_snprintf(instances, sizeof(instances),  " %d instances", cl_view.num_sprite_instances);
			g_snprintf(draw_elements, sizeof(draw_elements), " %d draw elements", r_stats.count_sprite_draw_elements);
		}

		R_Draw2DString(x, y, sprites, color_yellow);
		y += ch;
		R_Draw2DString(x, y, beams, color_yellow);
		y += ch;
		R_Draw2DString(x, y, instances, color_yellow);
		y += ch;
		R_Draw2DString(x, y, draw_elements, color_yellow);
		y += ch;
	}

	y += ch;

	{
		R_Draw2DString(x, y, "Draw 2D:", color_yellow);
		y += ch;
		R_Draw2DString(x, y, va("%d chars", r_stats.count_draw_chars), color_yellow);
		y += ch;
		R_Draw2DString(x, y, va("%d fills", r_stats.count_draw_fills), color_yellow);
		y += ch;
		R_Draw2DString(x, y, va("%d images", r_stats.count_draw_images), color_yellow);
		y += ch;
		R_Draw2DString(x, y, va("%d lines", r_stats.count_draw_lines), color_yellow);
		y += ch;
		R_Draw2DString(x, y, va("%d arrays", r_stats.count_draw_arrays), color_yellow);
		y += ch;
	}

	y += ch;

	{
		R_Draw2DString(x, y, "Other:", color_yellow);
		y += ch;
		R_Draw2DString(x, y, va("%d lights", cl_view.num_lights), color_yellow);
		y += ch;
	}

	const vec3_t forward = Vec3_Fmaf(cl_view.origin, MAX_WORLD_DIST, cl_view.forward);
	const cm_trace_t tr = Cl_Trace(cl_view.origin, forward, Box_Zero(), 0, CONTENTS_MASK_VISIBLE);

	if (tr.fraction < 1.f) {
		y += ch;

		const int32_t texinfo = tr.texinfo ? (int32_t) (ptrdiff_t) (tr.texinfo - Cm_Bsp()->texinfos) : -1;
		R_Draw2DString(x, y, va("%s %d (%g %g %g) %g",
								tr.texinfo->name,
								texinfo,
								tr.plane.normal.x, tr.plane.normal.y, tr.plane.normal.z,
								tr.plane.dist
								), color_white);
		y += ch;
	}

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws counters and performance information about the sound subsystem.
 */
static void Cl_DrawSoundStats(void) {
	r_pixel_t ch, x = 1, y = cl_draw_renderer_stats->value ? 540 : 64;

	if (!cl_draw_sound_stats->value) {
		return;
	}

	if (cls.state != CL_ACTIVE) {
		return;
	}

	if (cls.key_state.dest != KEY_GAME) {
		return;
	}

	R_BindFont("small", NULL, &ch);

	R_Draw2DString(x, y, "Sound:", color_magenta);
	y += ch;

	R_Draw2DString(x, y, va("%d channels", s_context.num_active_channels), color_magenta);
	y += ch;

	for (int32_t i = 0; i < MAX_CHANNELS; i++) {
		const s_channel_t *channel = &s_context.channels[i];

		if (!channel->play.sample) {
			continue;
		}

		ALenum state;
		alGetSourcei(s_context.sources[i], AL_SOURCE_STATE, &state);

		if (state != AL_PLAYING)
			continue;

		R_Draw2DString(x + ch, y, va("%i: %s @ (%f %f %f) : %i", i, channel->play.sample->media.name, channel->play.origin.x, channel->play.origin.y, channel->play.origin.z, channel->play.flags), color_magenta);
		y += ch;
	}

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief 
 */
static void Cl_DrawSampleCounter(char *buffer, gulong buffer_length, const char *title, uint16_t *samples) {
	uint16_t min, max;

	if (!cl.sample_count) {
		min = max = samples[cl.sample_index];
	} else {
		min = UINT16_MAX;
		max = 0;

		for (uint32_t i = 0; i < cl.sample_count; i++) {
			int32_t index = (cl.sample_index - i);

			if (index < 0) {
				index = STAT_COUNTER_SAMPLE_COUNT - (-index);
			}

			uint16_t sample = samples[index];
			min = min(min, sample);
			max = max(max, sample);
		}
	}

	g_snprintf(buffer, buffer_length, "%3u%s (^1%3u ^2%3u^7)", samples[cl.sample_index], title, min, max);
}

/**
 * @brief
 */
static void Cl_DrawCounters(void) {
	static vec3_t velocity;
	static char pps[28], fps[28], spd[8];
	static int32_t last_draw_time, last_speed_time;
	r_pixel_t cw, ch;

	if (!cl_draw_counters->integer) {
		return;
	}

	R_BindFont("small", &cw, &ch);

	r_pixel_t x = r_context.width - 7 * cw;
	r_pixel_t y = r_context.height - 3 * ch;

	cl.frame_counter[cl.sample_index]++;

	if (quetoo.ticks - last_speed_time >= 100) {

		velocity = cl.frame.ps.pm_state.velocity;
		velocity.z = 0.0;

		g_snprintf(spd, sizeof(spd), "%4.0fspd", Vec3_Length(velocity));

		last_speed_time = quetoo.ticks;
	}

	if (quetoo.ticks - last_draw_time >= 1000) {
		
		Cl_DrawSampleCounter(fps, sizeof(fps), "fps", cl.frame_counter);
		Cl_DrawSampleCounter(pps, sizeof(pps), "pps", cl.packet_counter);

		last_draw_time = quetoo.ticks;

		cl.sample_index = (cl.sample_index + 1) % STAT_COUNTER_SAMPLE_COUNT;
		cl.sample_count = min(STAT_COUNTER_SAMPLE_COUNT, cl.sample_count + 1);

		cl.frame_counter[cl.sample_index] = 0;
		cl.packet_counter[cl.sample_index] = 0;
	}

	if (cl_draw_position->integer) {
		R_Draw2DString(r_context.width - 14 * cw, y - ch, va("%4.0f %4.0f %4.0f",
														   cl.frame.ps.pm_state.origin.x,
														   cl.frame.ps.pm_state.origin.y,
														   cl.frame.ps.pm_state.origin.z), color_white);
	}

	R_Draw2DString(x, y, spd, color_white);
	y += ch;

	x = r_context.width - 16 * cw;

	R_Draw2DString(x, y, fps, color_white);
	y += ch;

	R_Draw2DString(x, y, pps, color_white);

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief This is called at least once per frame, and more often during loading.
 */
void Cl_UpdateScreen(void) {

	static cl_key_dest_t previous_key_dest = KEY_UI;
	if (cls.key_state.dest == KEY_UI) {
		if (previous_key_dest != KEY_UI) {
			Ui_ViewWillAppear();
		}
	} else {
		if (previous_key_dest == KEY_UI) {
			Ui_ViewWillDisappear();
		}
	}
	previous_key_dest = cls.key_state.dest;

	switch (cls.state) {
		case CL_UNINITIALIZED:
		case CL_DISCONNECTED:
		case CL_CONNECTING:
		case CL_CONNECTED:
			switch (cls.key_state.dest) {
				case KEY_UI:
					Ui_Draw();
					break;
				case KEY_CONSOLE:
					Cl_DrawConsole();
					break;
				default:
					break;
			}
			break;

		case CL_LOADING:
			Ui_Draw();
			break;

		case CL_ACTIVE:
			Cl_DrawNetGraph();
			Cl_DrawCounters();

			cls.cgame->UpdateScreen(&cl.frame);

			switch (cls.key_state.dest) {
				case KEY_UI:
					Ui_Draw();
					break;
				case KEY_CONSOLE:
					Cl_DrawConsole();
					break;
				case KEY_GAME:
					Cl_DrawChat();
					Cl_DrawNotify();
					Cl_DrawRendererStats();
					Cl_DrawSoundStats();
					break;
				case KEY_CHAT:
					Cl_DrawChat();
					break;
			}
			break;
	}
}
