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

#include "r_local.h"

static cvar_t *r_sprite_soften;

/**
 * @brief
 */
static struct {
  r_sprite_vertex_t vertexes[MAX_SPRITE_INSTANCES * 4];

  GLuint vertex_array;
  GLuint vertex_buffer;
  GLuint elements_buffer;

} r_sprites;

/**
 * @brief The draw program.
 */
static struct {
  GLuint name;
  
  GLuint uniforms_block;
  GLuint lights_block;

  GLint active_lights;

  GLint texture_diffusemap;
  GLint texture_next_diffusemap;
  GLint texture_voxel_diffuse;
  GLint texture_voxel_fog;
  GLint texture_depth_attachment_copy;

} r_sprite_program;

/**
 * @brief
 */
static void R_SpriteTextureCoordinates(const r_image_t *image, vec2_t *tl, vec2_t *tr, vec2_t *br, vec2_t *bl) {

  if (image->media.type == R_MEDIA_ATLAS_IMAGE) {
    const r_atlas_image_t *atlas_image = (r_atlas_image_t *) image;
    *tl = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.y);
    *tr = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.y);
    *br = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.w);
    *bl = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.w);
  } else {
    *tl = Vec2(0.f, 0.f);
    *tr = Vec2(1.f, 0.f);
    *br = Vec2(1.f, 1.f);
    *bl = Vec2(0.f, 1.f);
  }
}

/**
 * @brief
 */
static const r_image_t *R_ResolveSpriteImage(const r_media_t *media, const float life) {

  const r_image_t *image;

  if (media->type == R_MEDIA_ANIMATION) {
    image = R_ResolveAnimation((r_animation_t *) media, life, 0);
  } else {
    image = (r_image_t *) media;
  }

  return image;
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test, and returns a pointer to the sprite
 * slot it fills.
 */
r_sprite_t *R_AddSprite(r_view_t *view, const r_sprite_t *s) {

  assert(s->media);

  if (view->num_sprites == MAX_SPRITES) {
    Com_Debug(DEBUG_RENDERER, "MAX_SPRITES\n");
    return NULL;
  }

  const float radius = (s->size ?: Maxf(s->width, s->height)) * .5f;

  if (R_CulludeSphere(view, s->origin, radius)) {
    return NULL;
  }

  r_sprite_t *out = &view->sprites[view->num_sprites++];
  *out = *s;

  return out;
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test, and returns a pointer to the sprite
 * slot it fills.
 */
r_beam_t *R_AddBeam(r_view_t *view, const r_beam_t *b) {

  if (view->num_beams == MAX_BEAMS) {
    Com_Debug(DEBUG_RENDERER, "MAX_BEAMS\n");
    return NULL;
  }

  const box3_t bounds = Cm_TraceBounds(b->start, b->end, Box3f(b->size, b->size, b->size));

  if (R_CulludeBox(view, bounds)) {
    return NULL;
  }

  r_beam_t *out = &view->beams[view->num_beams++];
  *out = *b;

  return out;
}

/**
 * @brief
 */
static r_sprite_instance_t *R_AllocSpriteInstance(r_view_t *view) {

  if (view->num_sprite_instances == MAX_SPRITE_INSTANCES) {
    Com_Debug(DEBUG_RENDERER, "MAX_SPRITE_INSTANCES\n");
    return NULL;
  }

  const int32_t index = view->num_sprite_instances++;

  r_sprite_instance_t *in = &view->sprite_instances[index];
  memset(in, 0, sizeof(*in));

  in->vertexes = r_sprites.vertexes + 4 * index;
  in->elements = (GLvoid *) (sizeof(GLuint) * 6 * index);

  return in;
}

/**
 * @brief
 */
static void R_UpdateSprite(r_view_t *view, const r_sprite_t *s) {

  r_sprite_instance_t *in = R_AllocSpriteInstance(view);
  if (!in) {
    return;
  }

  in->flags = s->flags;
  in->diffusemap = R_ResolveSpriteImage(s->media, s->life);

  const float aspect_ratio = (float) in->diffusemap->width / (float) in->diffusemap->height;
  const float half_width = (s->size ?: s->width) * .5f;
  const float half_height = (s->size ?: s->height) * .5f;

  vec3_t dir, right, up;

  if (Vec3_Equal(s->dir, Vec3_Zero())) {

    if (s->axis == SPRITE_AXIS_ALL) {
      if (s->rotation) {
        dir = view->angles;
        dir.z = Degrees(s->rotation);
        Vec3_Vectors(dir, NULL, &right, &up);
      } else {
        right = view->right;
        up = view->up;
      }
    } else {
      dir = Vec3_Zero();

      for (int32_t i = 0; i < 3; i++) {
        if (s->axis & (1 << i)) {
          dir.xyz[i] = view->forward.xyz[i];
        }
      }

      dir = Vec3_Euler(Vec3_Normalize(dir));
      dir.z = Degrees(s->rotation);
      Vec3_Vectors(dir, NULL, &right, &up);
    }
  } else {
    dir = Vec3_Euler(s->dir);
    dir.z = Degrees(s->rotation);
    Vec3_Vectors(dir, NULL, &right, &up);
  }

  const vec3_t u = Vec3_Scale(up, half_height),
         d = Vec3_Scale(up, -half_height),
         l = Vec3_Scale(right, -half_width * aspect_ratio),
         r = Vec3_Scale(right, half_width * aspect_ratio);

  in->vertexes[0].position = Vec3_Add(Vec3_Add(s->origin, u), l);
  in->vertexes[1].position = Vec3_Add(Vec3_Add(s->origin, u), r);
  in->vertexes[2].position = Vec3_Add(Vec3_Add(s->origin, d), r);
  in->vertexes[3].position = Vec3_Add(Vec3_Add(s->origin, d), l);

  R_SpriteTextureCoordinates(in->diffusemap, &in->vertexes[0].diffusemap,
                         &in->vertexes[1].diffusemap,
                         &in->vertexes[2].diffusemap,
                         &in->vertexes[3].diffusemap);

  if (s->media->type == R_MEDIA_ANIMATION) {
    const r_animation_t *anim = (const r_animation_t *) s->media;

    in->next_diffusemap = R_ResolveAnimation(anim, s->life, 1);

    R_SpriteTextureCoordinates(in->next_diffusemap, &in->vertexes[0].next_diffusemap,
                            &in->vertexes[1].next_diffusemap,
                            &in->vertexes[2].next_diffusemap,
                            &in->vertexes[3].next_diffusemap);

    const float frame = s->life * anim->num_frames;
    const float lerp = Clampf01(frame - floorf(frame));

    in->vertexes[0].lerp =
    in->vertexes[1].lerp =
    in->vertexes[2].lerp =
    in->vertexes[3].lerp = lerp;
  } else {
    in->next_diffusemap = in->diffusemap;

    in->vertexes[0].next_diffusemap = in->vertexes[0].diffusemap;
    in->vertexes[1].next_diffusemap = in->vertexes[1].diffusemap;
    in->vertexes[2].next_diffusemap = in->vertexes[2].diffusemap;
    in->vertexes[3].next_diffusemap = in->vertexes[3].diffusemap;

    in->vertexes[0].lerp =
    in->vertexes[1].lerp =
    in->vertexes[2].lerp =
    in->vertexes[3].lerp = 0.f;
  }

  in->vertexes[0].color =
  in->vertexes[1].color =
  in->vertexes[2].color =
  in->vertexes[3].color = s->color;

  in->vertexes[0].softness =
  in->vertexes[1].softness =
  in->vertexes[2].softness =
  in->vertexes[3].softness = r_sprite_soften->value * s->softness;

  in->vertexes[0].lighting =
  in->vertexes[1].lighting =
  in->vertexes[2].lighting =
  in->vertexes[3].lighting = s->lighting;

  in->bounds = Box3_FromPointsStride(in->vertexes, 4, sizeof(r_sprite_vertex_t));
}

/**
 * @brief Break the beam into segments based on blend depth transitions.
 */
void R_UpdateBeam(r_view_t *view, const r_beam_t *b) {
  float length;

  const vec3_t up = Vec3_NormalizeLength(Vec3_Subtract(b->start, b->end), &length);
  length /= b->image->width * (b->size / b->image->height);

  const float half_size = b->size * .5f;
  const vec3_t right = Vec3_Scale(Vec3_Normalize(Vec3_Cross(up, Vec3_Subtract(view->origin, b->end))), half_size);

  vec2_t texcoords[4];
  R_SpriteTextureCoordinates(b->image, &texcoords[0], &texcoords[1], &texcoords[2], &texcoords[3]);

  if (b->flags & SPRITE_BEAM_REPEAT) {

    if (b->stretch) {
      length *= b->stretch;
    }

    texcoords[1].x *= length;
    texcoords[2].x = texcoords[1].x;

    if (b->translate) {
      for (int32_t i = 0; i < 4; i++) {
        texcoords[i].x += b->translate;
      }
    }
  }

  float step = 1.f;
  for (float frac = 0.f; frac < 1.f; ) {

    const vec3_t x = Vec3_Mix(b->start, b->end, frac);
    const vec3_t y = Vec3_Mix(b->start, b->end, frac + step);

    vec3_t positions[4];
    positions[0] = Vec3_Add(x, right);
    positions[1] = Vec3_Add(y, right);
    positions[2] = Vec3_Subtract(y, right);
    positions[3] = Vec3_Subtract(x, right);

    r_sprite_instance_t *in = R_AllocSpriteInstance(view);
    if (!in) {
      return;
    }

    in->flags = b->flags;
    in->diffusemap = in->next_diffusemap = b->image;

    in->vertexes[0].position = positions[0];
    in->vertexes[1].position = positions[1];
    in->vertexes[2].position = positions[2];
    in->vertexes[3].position = positions[3];

    in->vertexes[0].diffusemap = in->vertexes[0].next_diffusemap = texcoords[0];
    in->vertexes[1].diffusemap = in->vertexes[1].next_diffusemap = texcoords[1];
    in->vertexes[2].diffusemap = in->vertexes[2].next_diffusemap = texcoords[2];
    in->vertexes[3].diffusemap = in->vertexes[3].next_diffusemap = texcoords[3];

    const float xs = (texcoords[0].x * (1.f - frac)) + (texcoords[1].x * frac);
    const float ys = (texcoords[0].x * (1.f - (frac + step))) + (texcoords[1].x * (frac + step));

    in->vertexes[0].diffusemap.x = in->vertexes[0].next_diffusemap.x = xs;
    in->vertexes[1].diffusemap.x = in->vertexes[1].next_diffusemap.x = ys;
    in->vertexes[2].diffusemap.x = in->vertexes[2].next_diffusemap.x = ys;
    in->vertexes[3].diffusemap.x = in->vertexes[3].next_diffusemap.x = xs;

    in->vertexes[0].color =
    in->vertexes[1].color =
    in->vertexes[2].color =
    in->vertexes[3].color = b->color;

    in->vertexes[0].lerp =
    in->vertexes[1].lerp =
    in->vertexes[2].lerp =
    in->vertexes[3].lerp = 0.f;

    in->vertexes[0].softness =
    in->vertexes[1].softness =
    in->vertexes[2].softness =
    in->vertexes[3].softness = r_sprite_soften->value * b->softness;

    in->vertexes[0].lighting =
    in->vertexes[1].lighting =
    in->vertexes[2].lighting =
    in->vertexes[3].lighting = b->lighting;

    in->bounds = Box3_FromPointsStride(in->vertexes, 4, sizeof(r_sprite_vertex_t));

    frac += step;
    step = 1.f - frac;
  }
}

/**
 * @brief Generate sprite instances from sprites and beams, and update the vertex array.
 */
void R_UpdateSprites(r_view_t *view) {

  R_AddBspVoxelSprites(view);
  
  const r_sprite_t *s = view->sprites;
  for (int32_t i = 0; i < view->num_sprites; i++, s++) {
    R_UpdateSprite(view, s);
  }

  const r_beam_t *b = view->beams;
  for (int32_t i = 0; i < view->num_beams; i++, b++) {
    R_UpdateBeam(view, b);
  }
}

/**
 * @brief
 */
void R_DrawSprites(const r_view_t *view) {

  assert(view->framebuffer);

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  glUseProgram(r_sprite_program.name);

  glBindBuffer(GL_ARRAY_BUFFER, r_sprites.vertex_buffer);

  const GLsizei count = view->num_sprite_instances * 4;
  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(r_sprite_vertex_t), r_sprites.vertexes);

  glBindVertexArray(r_sprites.vertex_array);

  if (r_sprite_soften->value) {
    R_CopyFramebufferAttachment(view->framebuffer, ATTACHMENT_DEPTH, &view->framebuffer->depth_attachment_copy);
  }

  glActiveTexture(GL_TEXTURE0 + TEXTURE_DEPTH_ATTACHMENT_COPY);
  glBindTexture(GL_TEXTURE_2D, view->framebuffer->depth_attachment_copy);

  int32_t i = 0;
  while (i < view->num_sprite_instances) {

    const r_sprite_instance_t *in = view->sprite_instances + i;

    glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);
    glBindTexture(GL_TEXTURE_2D, in->diffusemap->texnum);

    glActiveTexture(GL_TEXTURE0 + TEXTURE_NEXT_DIFFUSEMAP);
    glBindTexture(GL_TEXTURE_2D, in->next_diffusemap->texnum);

    GLsizei batch_size = 1;

    box3_t bounds = in->bounds;

    const r_sprite_instance_t *batch = in + 1;
    for (int32_t j = i + 1; j < view->num_sprite_instances; j++, batch++) {

      if (batch->diffusemap == in->diffusemap &&
        batch->next_diffusemap == in->next_diffusemap) {
        bounds = Box3_Union(bounds, in->bounds);
        batch_size++;
      } else {
        break;
      }
    }

    R_ActiveLights(view, bounds, r_sprite_program.active_lights);

    glDrawElements(GL_TRIANGLES, batch_size * 6, GL_UNSIGNED_INT, in->elements);
    r_stats.sprite_draw_elements++;

    i += batch_size;
  }

  glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

  glBindVertexArray(0);

  glUseProgram(0);

  glBlendFunc(GL_ONE, GL_ZERO);
  glDisable(GL_BLEND);

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitSpriteProgram(void) {

  memset(&r_sprite_program, 0, sizeof(r_sprite_program));

  r_sprite_program.name = R_LoadProgram(
      R_ShaderDescriptor(GL_VERTEX_SHADER, "sprite_vs.glsl", NULL),
      R_ShaderDescriptor(GL_FRAGMENT_SHADER, "soften_fs.glsl", "sprite_fs.glsl", NULL),
      NULL);
  
  glUseProgram(r_sprite_program.name);

  r_sprite_program.uniforms_block = glGetUniformBlockIndex(r_sprite_program.name, "uniforms_block");
  glUniformBlockBinding(r_sprite_program.name, r_sprite_program.uniforms_block, 0);

  r_sprite_program.lights_block = glGetUniformBlockIndex(r_sprite_program.name, "lights_block");
  glUniformBlockBinding(r_sprite_program.name, r_sprite_program.lights_block, 1);

  r_sprite_program.active_lights = glGetUniformLocation(r_sprite_program.name, "active_lights");

  r_sprite_program.texture_diffusemap = glGetUniformLocation(r_sprite_program.name, "texture_diffusemap");
  r_sprite_program.texture_next_diffusemap = glGetUniformLocation(r_sprite_program.name, "texture_next_diffusemap");
  r_sprite_program.texture_voxel_diffuse = glGetUniformLocation(r_sprite_program.name, "texture_voxel_diffuse");
  r_sprite_program.texture_voxel_fog = glGetUniformLocation(r_sprite_program.name, "texture_voxel_fog");
  r_sprite_program.texture_depth_attachment_copy = glGetUniformLocation(r_sprite_program.name, "texture_depth_attachment_copy");

  glUniform1i(r_sprite_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);
  glUniform1i(r_sprite_program.texture_next_diffusemap, TEXTURE_NEXT_DIFFUSEMAP);
  glUniform1i(r_sprite_program.texture_voxel_diffuse, TEXTURE_VOXEL_DIFFUSE);
  glUniform1i(r_sprite_program.texture_voxel_fog, TEXTURE_VOXEL_FOG);
  glUniform1i(r_sprite_program.texture_depth_attachment_copy, TEXTURE_DEPTH_ATTACHMENT_COPY);

  glUseProgram(0);

  R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitSprites(void) {

  memset(&r_sprites, 0, sizeof(r_sprites));

  glGenVertexArrays(1, &r_sprites.vertex_array);
  glBindVertexArray(r_sprites.vertex_array);

  glGenBuffers(1, &r_sprites.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, r_sprites.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(r_sprites.vertexes), NULL, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &r_sprites.elements_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_sprites.elements_buffer);

  GLuint elements[MAX_SPRITES * 6];
  for (GLuint i = 0, v = 0, e = 0; i < MAX_SPRITES; i++, v += 4, e += 6) {
    elements[e + 0] = v + 0;
    elements[e + 1] = v + 1;
    elements[e + 2] = v + 2;
    elements[e + 3] = v + 0;
    elements[e + 4] = v + 2;
    elements[e + 5] = v + 3;
  }
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);
  
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, position));
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, diffusemap));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, next_diffusemap));
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, color));
  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, lerp));
  glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, softness));
  glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(r_sprite_vertex_t), (void *) offsetof(r_sprite_vertex_t, lighting));

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glEnableVertexAttribArray(4);
  glEnableVertexAttribArray(5);
  glEnableVertexAttribArray(6);

  glBindVertexArray(0);

  R_GetError(NULL);

  R_InitSpriteProgram();
  
  r_sprite_soften = Cvar_Add("r_sprite_soften", "1", 0, "Whether sprite softening is enabled");
}

/**
 * @brief
 */
static void R_ShutdownSpriteProgram(void) {

  glDeleteProgram(r_sprite_program.name);

  r_sprite_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownSprites(void) {

  glDeleteVertexArrays(1, &r_sprites.vertex_array);
  glDeleteBuffers(1, &r_sprites.vertex_buffer);
  glDeleteBuffers(1, &r_sprites.elements_buffer);

  R_ShutdownSpriteProgram();
}
