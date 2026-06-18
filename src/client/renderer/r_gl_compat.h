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

/**
 * @file r_gl_compat.h
 * @brief OpenGL 4.1 Core vs OpenGL ES 3.0 compatibility seam.
 *
 * This header centralizes the handful of places where the desktop renderer uses
 * GL functionality that is absent or different in OpenGL ES 3.0, so that the hot
 * rendering code stays free of per-call `#ifdef QUETOO_GLES` blocks. It is
 * included from `r_local.h` immediately after the GL loader header.
 *
 * Each shim presents the exact desktop signature the renderer already calls and
 * collapses the dual path into a single inline helper:
 *   - on desktop it forwards to the native GL 4.1 entry point;
 *   - under `QUETOO_GLES` it forwards to the ES-3.0-legal equivalent, a no-op, or
 *     a TODO stub for the larger emulations.
 *
 * The full audit lives in android/RENDERER_GLES.md; the Finding letters below
 * (A, G, H, I, ...) reference its severity table.
 */

#ifdef QUETOO_GLES
// TODO(QUETOO_GLES, #856): include the parallel glad GLES 3.0 loader header here
// once it is generated (RENDERER_GLES.md Finding K / §3.1). The desktop build
// pulls the loader in via r_local.h -> r_gl.h before this file; the ES build will
// instead select the gles2:3.0 header. Until then the ES path does not link, which
// is expected for this foundational pass.
#endif

/**
 * @brief Compile-time flag, true when building the OpenGL ES 3.0 renderer.
 */
#ifdef QUETOO_GLES
	#define R_GLES 1
#else
	#define R_GLES 0
#endif

/**
 * @brief Sets the polygon rasterization mode (Finding A).
 *
 * Desktop uses `glPolygonMode` for the `r_draw_wireframe` debug view. ES 3.0 has
 * no `glPolygonMode` at all, so wireframe is dropped per #856 (a real ES wireframe
 * needs a barycentric geometry trick, out of scope). The `mode` argument keeps the
 * desktop call sites verbatim.
 */
static inline void R_SetPolygonMode(GLenum mode) {
#ifdef QUETOO_GLES
	(void) mode; // No glPolygonMode in ES 3.0; wireframe debug view is dropped (#856).
#else
	glPolygonMode(GL_FRONT_AND_BACK, mode);
#endif
}

/**
 * @brief Configures a depth-only framebuffer to draw to no color buffer (Finding H).
 *
 * Desktop uses the singular `glDrawBuffer(GL_NONE)`, which does not exist in ES.
 * ES provides only the plural `glDrawBuffers`, which the rest of the renderer
 * already uses, so this is a trivial swap that is valid on both profiles.
 */
static inline void R_DrawBufferNone(void) {
#ifdef QUETOO_GLES
	glDrawBuffers(1, (const GLenum []) { GL_NONE });
#else
	glDrawBuffer(GL_NONE);
#endif
}

/**
 * @brief Enables or disables depth clamping (Finding G).
 *
 * Desktop enables `GL_DEPTH_CLAMP` around the shadow-atlas pass to avoid
 * light-frustum near-plane clipping. ES 3.0 has no depth clamp (no guaranteed
 * NV/EXT_depth_clamp), so this is a no-op there; the accepted trade-off per #856
 * is mild "Peter Panning" (shadows detaching slightly from occluders).
 */
static inline void R_SetDepthClamp(_Bool enable) {
#ifdef QUETOO_GLES
	(void) enable; // No GL_DEPTH_CLAMP in ES 3.0; accept Peter Panning (#856).
#else
	if (enable) {
		glEnable(GL_DEPTH_CLAMP);
	} else {
		glDisable(GL_DEPTH_CLAMP);
	}
#endif
}

/**
 * @brief Probes for anisotropic-filtering support (Finding I).
 *
 * On desktop anisotropy is core via GL_ARB_texture_filter_anisotropic and always
 * available. On ES 3.0 it is the optional EXT_texture_filter_anisotropic
 * extension; setting GL_TEXTURE_MAX_ANISOTROPY without it raises GL_INVALID_ENUM.
 * Callers should gate both the max-anisotropy query and the per-texture parameter
 * on this probe.
 */
static inline _Bool R_HasAnisotropy(void) {
#ifdef QUETOO_GLES
	// TODO(QUETOO_GLES, #856): replace with a real runtime extension probe
	// (scan glGetStringi(GL_EXTENSIONS, i) up to GL_NUM_EXTENSIONS for
	// "GL_EXT_texture_filter_anisotropic"; cache in an r_gl_config struct).
	// RENDERER_GLES.md Finding I / §3.2. Returning false keeps anisotropy off
	// (r_anisotropy effectively 1) until the probe lands.
	return false;
#else
	return true;
#endif
}

/* Desktop GL enums that glad supplied but GLES 3.0 core headers lack. Define the
 * (identical) values so the shared renderer compiles on ES; their call sites are
 * dead or repacked on ES -- anisotropy is off (R_HasAnisotropy() false, Finding I)
 * and the texture-buffer (TBO) voxel-light path is repacked at map load (Finding D),
 * never reached by the 2D UI. */
#if defined(QUETOO_GLES)
#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_TEXTURE_BUFFER
#define GL_TEXTURE_BUFFER 0x8C2A
#endif
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_LINE
#define GL_LINE 0x1B01
#endif
#ifndef GL_FILL
#define GL_FILL 0x1B02
#endif
#ifndef GL_TEXTURE_WIDTH
#define GL_TEXTURE_WIDTH 0x1000
#endif
#ifndef GL_TEXTURE_HEIGHT
#define GL_TEXTURE_HEIGHT 0x1001
#endif
#ifndef GL_TEXTURE_DEPTH
#define GL_TEXTURE_DEPTH 0x8071
#endif
/* No-op shims for desktop-GL entry points absent in ES 3.0. Their call sites are
 * non-2D-UI paths (wireframe debug, TBO voxel light, texture readback/screenshots)
 * that are dead or repacked on ES (Findings A, C, D); they never run for the menu.
 * gl3.h (ES 3.0) declares none of these, so the definitions don't conflict. */
static inline void glPolygonMode(GLenum face, GLenum mode) { (void) face; (void) mode; }
static inline void glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
	(void) target; (void) internalformat; (void) buffer;
}
static inline void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {
	(void) target; (void) level; (void) pname; if (params) { *params = 0; }
}
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif
/* ES provides glDepthRangef (float); desktop glDepthRange takes doubles. Forward
 * to the real ES entry point (this one is live -- used in mesh depth-hacking). */
static inline void glDepthRange(double n, double f) { glDepthRangef((float) n, (float) f); }
/* ES 3.0 has glGetQueryObjectuiv (unsigned) but not the signed glGetQueryObjectiv. */
static inline void glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params) {
	glGetQueryObjectuiv(id, pname, (GLuint *) params);
}
#endif

/**
 * @brief Sets the max-anisotropy sampler parameter on the bound texture (Finding I).
 *
 * No-op when anisotropy is unavailable (ES without the extension), which keeps the
 * single call site in R_SetupImage clean.
 */
static inline void R_SetAnisotropy(GLenum target, GLfloat anisotropy) {
	if (R_HasAnisotropy()) {
		glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY, anisotropy);
	} else {
		(void) target;
		(void) anisotropy;
	}
}

/**
 * @brief Draws indexed primitives with a per-call base vertex (Finding C).
 *
 * Desktop has `glDrawElementsBaseVertex` (core in 4.1). It is GL ES 3.2, NOT 3.0,
 * so it must be emulated on ES by baking the base vertex into the indices at load
 * time, or by re-pointing the bound array buffer before a plain glDrawElements.
 * Wrapped here so every hot call site (r_mesh_draw.c, r_shadow.c, r_occlude.c)
 * stays unchanged.
 */
static inline void R_DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                            const GLvoid *indices, GLint base_vertex) {
#ifdef QUETOO_GLES
	// ES 3.0 lacks glDrawElementsBaseVertex (it is core in ES 3.2 / GL 3.2). Modern
	// ES 3.1+ GPUs expose it via the core 3.2 entry or the OES/EXT extension, so we
	// resolve it once at runtime and call the real thing. This is critical, not
	// cosmetic: the occlusion-query bounding-box draw (r_occlude.c) routes through
	// here, so a no-op makes every query record zero samples -> every BSP block is
	// reported occluded -> the entire world renders black. It also draws all mesh
	// models (r_mesh_draw.c) and mesh shadows (r_shadow.c). (#856, Finding C.)
	typedef void (*r_pfn_draw_elements_base_vertex)(GLenum, GLsizei, GLenum, const GLvoid *, GLint);
	static r_pfn_draw_elements_base_vertex draw_base_vertex = NULL;
	static bool draw_base_vertex_resolved = false;
	if (!draw_base_vertex_resolved) {
		draw_base_vertex_resolved = true; // GL is single-threaded; resolve lazily once
		draw_base_vertex = (r_pfn_draw_elements_base_vertex) SDL_GL_GetProcAddress("glDrawElementsBaseVertex");
		if (draw_base_vertex == NULL) {
			draw_base_vertex = (r_pfn_draw_elements_base_vertex) SDL_GL_GetProcAddress("glDrawElementsBaseVertexOES");
		}
		if (draw_base_vertex == NULL) {
			draw_base_vertex = (r_pfn_draw_elements_base_vertex) SDL_GL_GetProcAddress("glDrawElementsBaseVertexEXT");
		}
	}
	if (draw_base_vertex) {
		draw_base_vertex(mode, count, type, indices, base_vertex);
	} else {
		// Pre-3.2 ES with no extension: a plain draw is correct only for
		// base_vertex == 0; ES 3.2 devices (e.g. Adreno 750) take the path above.
		glDrawElements(mode, count, type, indices);
	}
#else
	glDrawElementsBaseVertex(mode, count, type, indices, base_vertex);
#endif
}

/**
 * @brief Reads a texture image level back to client memory (Findings E + F).
 *
 * Desktop uses `glGetTexImage`, which does not exist in ES (ES can only read from a
 * framebuffer via glReadPixels). `format` may also be `GL_BGR`, which ES glReadPixels
 * does not accept. Used only by the texture/material dump tool and screenshots, so
 * correctness over speed.
 */
static inline void R_GetTextureImage(GLenum target, GLint level, GLenum format,
                                      GLenum pixel_type, GLvoid *pixels) {
#ifdef QUETOO_GLES
	// TODO(QUETOO_GLES, #856): emulate texture readback on ES 3.0.
	// RENDERER_GLES.md Findings E/F: attach the texture (or each cubemap face /
	// array layer / mip) to a temporary FBO via glFramebufferTexture2D /
	// glFramebufferTextureLayer, then glReadPixels as GL_RGBA/GL_UNSIGNED_BYTE and
	// swizzle to BGR on the CPU. Replace glGetTexLevelParameteriv dimension queries
	// with the CPU-side r_image_t width/height/depth (halving per mip). Developer/
	// screenshot tool only. Stubbed for now.
	(void) target; (void) level; (void) format; (void) pixel_type; (void) pixels;
#else
	glGetTexImage(target, level, format, pixel_type, pixels);
#endif
}
