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

#include <assert.h>

#include "MeshModelView.h"
#include "../../renderer/renderer.h"

#define _Class _MeshModelView

#pragma mark - View

/**
 * @see View::render(View *, SDL_Renderer *)
 */
static void render(View *self, SDL_Renderer *renderer) {

	super(View, self, render, renderer);

	MeshModelView *this = (MeshModelView *) self;

	if (this->model) {

		const SDL_Rect frame = $(self, renderFrame);
		glViewport(frame.x, frame.y, frame.w, frame.h);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glOrtho(-2.0, 2.0, -2.0, 2.0, -3.0, 3.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		// Quake is retarded: rotate so that Z is up
		glRotatef(90.0, 1.0, 0.0, 0.0);
		glRotatef(90.0, 0.0, 0.0, 1.0);
		glRotatef(15.0, 0.0, 1.0, 0.0);
		glTranslatef(0.0, 0.0, -1.0);

		glPushMatrix();

		glRotatef(quetoo.time * 0.08, 0.0, 0.0, 1.0);
		glScalef(this->scale, this->scale, this->scale);

		glColor4f(1.0, 1.0, 1.0, 1.0);

		const r_model_t *model = R_LoadModel(this->model);
		if (model) {

			assert(model->type == MOD_MD3);

			static SDL_Texture *texture;
			if (texture == NULL) {
				const char *image = model->mesh->material->diffuse->media.name;

				SDL_Surface *surface;
				if (Img_LoadImage(image, &surface)) {
					texture = SDL_CreateTextureFromSurface(renderer, surface);
				}
			}

			if (texture) {
				SDL_GL_BindTexture(texture, NULL, NULL);
			}

			const r_md3_t *md3 = (r_md3_t *) model->mesh->data;
			const d_md3_frame_t *frame = md3->frames;

			vec3_t verts[model->num_verts];

			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(3, GL_FLOAT, 0, verts);

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, 0, model->texcoords);

			const r_md3_mesh_t *mesh = md3->meshes;
			for (uint16_t i = 0; i < md3->num_meshes; i++, mesh++) {

				const r_md3_vertex_t *v = mesh->verts;

				for (uint16_t j = 0; j < mesh->num_verts; j++, v++) {
					VectorAdd(v->point, frame->translate, verts[j]);
				}

				glDrawElements(GL_TRIANGLES, mesh->num_tris * 3, GL_UNSIGNED_INT, mesh->tris);
			}

			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);

			SDL_GL_UnbindTexture(texture);

		} else {

			glBegin(GL_TRIANGLE_FAN);
			glVertex3f(0.0, 0.0, -16.0);
			for (int32_t i = 0; i <= 4; i++)
				glVertex3f(16.0 * cos(i * M_PI_2), 16.0 * sin(i * M_PI_2), 0.0);
			glEnd();

			glBegin(GL_TRIANGLE_FAN);
			glVertex3f(0.0, 0.0, 16.0);
			for (int32_t i = 4; i >= 0; i--)
				glVertex3f(16.0 * cos(i * M_PI_2), 16.0 * sin(i * M_PI_2), 0.0);
			glEnd();
		}

		glPopMatrix();
	}

	glViewport(0, 0, r_context.window_width, r_context.window_height);
}


#pragma mark - MeshModelView

/**
 * @fn MeshModelView *MeshModelView::init(MeshModelView *self)
 *
 * @memberof MeshModelView
 */
static MeshModelView *initWithFrame(MeshModelView *self, const SDL_Rect *frame) {

	self = (MeshModelView *) super(View, self, initWithFrame, frame);
	if (self) {

		self->scale = 1.0;

		self->view.backgroundColor = Colors.Charcoal;
		self->view.backgroundColor.a = 128;
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->render = render;

	((MeshModelViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

Class _MeshModelView = {
	.name = "MeshModelView",
	.superclass = &_View,
	.instanceSize = sizeof(MeshModelView),
	.interfaceOffset = offsetof(MeshModelView, interface),
	.interfaceSize = sizeof(MeshModelViewInterface),
	.initialize = initialize,
};

#undef _Class

