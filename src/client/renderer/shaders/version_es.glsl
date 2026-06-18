#version 300 es
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

// OpenGL ES 3.0 preamble. See android/RENDERER_GLES.md Finding J.
// This file replaces version.glsl when the engine is built with QUETOO_GLES
// (selected in r_program.c R_ShaderDescriptor). It gates all 29 shaders.
//
// The `#version` directive MUST be the very first line of the assembled shader
// (this file is concatenated first); SwiftShader/ES rejects any comment or blank
// line before it ("#version directive must occur on the first line").
//
// ES fragment shaders REQUIRE explicit default precision qualifiers for
// float/int and every sampler type used; desktop GLSL declares none, so they
// must live here so no per-shader edits are needed.

// ES 3.0 has no texture-buffer samplers (isamplerBuffer is GL 3.1+/ES 3.2). The
// voxel light-index TBO is repacked to an integer 2D texture on ES (Finding D),
// so remap the type for uniforms.glsl's declaration. Only voxel.glsl/bsp use it
// (3D world, compiled at map load); the 2D UI/menu shaders just see the decl.
#define isamplerBuffer isampler2D

precision highp float;
precision highp int;

precision highp sampler2D;
precision highp isampler2D;
precision highp sampler2DArray;
precision highp sampler2DArrayShadow;
precision highp samplerCube;
precision highp sampler3D;
precision highp isampler3D;

// TODO(QUETOO_GLES, #856): once the voxel light-index TBO is repacked to an
// integer 2D texture (RENDERER_GLES.md Finding D), add the matching default
// precision for that sampler (e.g. `precision highp isampler2D;`). The current
// desktop `isamplerBuffer` (uniforms.glsl:238) has no ES equivalent in 3.0.
