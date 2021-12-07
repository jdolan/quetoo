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

#define CONTENTS_LAVA  0x8
#define CONTENTS_SLIME 0x10
#define CONTENTS_WATER 0x20

#define CONTENTS_MASK_LIQUID (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER)

/**
 * @brief
 */
void caustics(in int contents, in vec3 point, inout vec3 diffuse) {

	float hz;
	switch (contents & CONTENTS_MASK_LIQUID) {
		case CONTENTS_LAVA:
			hz = 0.2;
			break;
		case CONTENTS_SLIME:
			hz = 0.3;
			break;
		case CONTENTS_WATER:
			hz = 0.5;
			break;
		default:
			return;
	}

	float thickness = 0.05;
	float glow = 3.0;
	float intensity = 0.3;

	// grab raw 3d noise
	float factor = noise3d((point * vec3(0.024, 0.024, 0.016)) + (ticks / 1000.0) * hz);

	// scale to make very close to -1.0 to 1.0 based on observational data
	factor = factor * (0.3515 * 2.0);

	// make the inner edges stronger, clamp to 0-1
	factor = clamp(pow((1.0 - abs(factor)) + thickness, glow), 0.0, 1.0);

	// add it up
	diffuse = clamp(diffuse + factor * intensity, 0.0, 1.0);
}
