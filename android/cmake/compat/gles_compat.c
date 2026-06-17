/*
 * gles_compat.c — link-time stubs for legacy OpenGL fixed-function entry points.
 *
 * ObjectivelyMVC's built-in Renderer (Sources/ObjectivelyMVC/Renderer.c) draws
 * with OpenGL 2.x immediate-mode / client-array calls that do not exist in
 * OpenGL ES 3.0. Quetoo installs its own Renderer subclass (QuetooRenderer,
 * src/client/ui/QuetooRenderer.c) that performs all drawing with modern GLES,
 * so MVC's fixed-function paths are dead code in the Quetoo client — but their
 * symbols are still referenced by libObjectivelyMVC.a and must resolve at link
 * time. These no-op definitions satisfy the linker. If a future change makes
 * MVC's base Renderer reachable at runtime, replace these with real GLES
 * emulation (a small VBO + shader).
 *
 * Android client-only; compiled into main.so/cgame.so only under QUETOO_GLES.
 */
#include <GLES3/gl3.h>

void glEnableClientState(GLenum cap) { (void) cap; }
void glDisableClientState(GLenum cap) { (void) cap; }

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
	(void) size; (void) type; (void) stride; (void) pointer;
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
	(void) size; (void) type; (void) stride; (void) pointer;
}

void glColor4ubv(const GLubyte *v) { (void) v; }

void glRecti(GLint x1, GLint y1, GLint x2, GLint y2) {
	(void) x1; (void) y1; (void) x2; (void) y2;
}
