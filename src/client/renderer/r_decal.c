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

#define MAX_DECAL_VERTS 2048
#define MAX_DECAL_ELEMENTS 4096

/**
 * @brief Decal vertex structure.
 */
typedef struct {
  vec3_t position;
  vec2_t texcoord;
  color32_t color;
} r_decal_vertex_t;

/**
 * @brief Decal storage and rendering state.
 */
static struct {
  /**
   * @brief The queue of allocated decals.
   */
  GQueue *allocated;

  /**
   * @brief The queue of free decals.
   */
  GQueue *free;

  /**
   * @brief The vertex buffer object.
   */
  GLuint vertex_buffer;

  /**
   * @brief The elements buffer object.
   */
  GLuint elements_buffer;

  /**
   * @brief The vertex array object.
   */
  GLuint vertex_array;

  /**
   * @brief Dynamic vertex buffer for decals.
   */
  r_decal_vertex_t vertexes[MAX_DECALS * 4];
  int32_t num_vertexes;

  /**
   * @brief Dynamic element buffer for decals.
   */
  GLuint elements[MAX_DECALS * 6];
  int32_t num_elements;
} r_decals;

/**
 * @brief The decal program.
 */
static struct {
  GLuint name;
  GLuint uniforms_block;
  GLint texture_diffusemap;
} r_decal_program;

/**
 * @brief Allocate a decal from the pool.
 */
static r_decal_t *R_AllocDecal(void) {

  r_decal_t *decal = g_queue_pop_head(r_decals.free);
  if (decal == NULL) {
    decal = Mem_TagMalloc(sizeof(r_decal_t), MEM_TAG_RENDERER);
  }

  g_queue_push_tail(r_decals.allocated, decal);
  return decal;
}

/**
 * @brief Free a decal back to the pool.
 */
static void R_FreeDecal(r_decal_t *decal) {
  g_queue_remove(r_decals.allocated, decal);
  g_queue_push_head(r_decals.free, decal);
}

/**
 * @brief Get texture coordinates for a decal, handling both regular images and atlas images.
 */
static void R_DecalTextureCoordinates(const r_media_t *media, vec2_t *tl, vec2_t *tr, vec2_t *br, vec2_t *bl) {

  if (media && media->type == R_MEDIA_ATLAS_IMAGE) {
    const r_atlas_image_t *atlas_image = (r_atlas_image_t *) media;
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
 * @brief Generate decal geometry on a BSP face.
 */
static void R_DecalFace(const r_decal_t *decal, r_bsp_face_t *face) {

  if (r_decals.num_vertexes + 4 > lengthof(r_decals.vertexes)) {
    return;
  }

  if (r_decals.num_elements + 6 > lengthof(r_decals.elements)) {
    return;
  }

  // Calculate decal basis vectors
  vec3_t tangent, bitangent;
  if (fabsf(decal->normal.z) > 0.9) {
    tangent = Vec3_Cross(decal->normal, Vec3(1.0, 0.0, 0.0));
  } else {
    tangent = Vec3_Cross(decal->normal, Vec3(0.0, 0.0, 1.0));
  }
  tangent = Vec3_Normalize(tangent);
  bitangent = Vec3_Cross(decal->normal, tangent);

  // Generate decal quad
  const vec3_t extents[4] = {
    Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent, -decal->radius)), Vec3_Scale(bitangent, -decal->radius)),
    Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent,  decal->radius)), Vec3_Scale(bitangent, -decal->radius)),
    Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent,  decal->radius)), Vec3_Scale(bitangent,  decal->radius)),
    Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent, -decal->radius)), Vec3_Scale(bitangent,  decal->radius))
  };

  vec2_t texcoords[4];
  R_DecalTextureCoordinates(decal->media, &texcoords[0], &texcoords[1], &texcoords[2], &texcoords[3]);

  // TODO: Clip decal quad to face bounds - for now just add the quad
  // A full implementation would intersect the quad with the face polygon

  const color32_t color = Color_Color32(decal->color);
  const int32_t base_vertex = r_decals.num_vertexes;

  for (int32_t i = 0; i < 4; i++) {
    r_decals.vertexes[r_decals.num_vertexes++] = (r_decal_vertex_t) {
      .position = extents[i],
      .texcoord = texcoords[i],
      .color = color
    };
  }

  r_decals.elements[r_decals.num_elements++] = base_vertex + 0;
  r_decals.elements[r_decals.num_elements++] = base_vertex + 1;
  r_decals.elements[r_decals.num_elements++] = base_vertex + 2;
  r_decals.elements[r_decals.num_elements++] = base_vertex + 0;
  r_decals.elements[r_decals.num_elements++] = base_vertex + 2;
  r_decals.elements[r_decals.num_elements++] = base_vertex + 3;
}

/**
 * @brief Recursively traverse BSP tree to project decal onto faces.
 */
static void R_DecalNode(const r_decal_t *decal, const r_bsp_node_t *node) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const cm_bsp_plane_t *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_DecalNode(decal, node->children[0]);
    return;
  }

  if (dist < -decal->radius) {
    R_DecalNode(decal, node->children[1]);
    return;
  }

  // Project the decal onto the node's plane
  const r_decal_t projected = {
    .origin = Vec3_Fmaf(decal->origin, -dist, plane->normal),
    .normal = decal->normal,
    .radius = decal->radius - fabsf(dist),
    .color = decal->color,
    .media = decal->media,
    .time = decal->time,
    .lifetime = decal->lifetime
  };

  r_bsp_face_t *face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {

    if (!(face->brush_side->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->brush_side->surface & (SURF_SKY | SURF_NO_DRAW)) {
      continue;
    }

    R_DecalFace(&projected, face);
  }

  // Recurse down both sides
  R_DecalNode(decal, node->children[0]);
  R_DecalNode(decal, node->children[1]);
}

/**
 * @brief Adds a decal to the persistent pool.
 */
void R_AddDecal(r_view_t *view, const r_decal_t *decal) {

  Com_Debug(DEBUG_RENDERER, "R_AddDecal: lifetime=%u, media=%p\n", decal->lifetime, (void*)decal->media);

  r_decal_t *d = R_AllocDecal();
  *d = *decal;
  
  Com_Debug(DEBUG_RENDERER, "  Added persistent decal (total=%u)\n", g_queue_get_length(r_decals.allocated));
}

/**
 * @brief Update persistent decals - remove expired ones and add active ones to view.
 */
void R_UpdateDecals(r_view_t *view) {

  // Remove expired decals
  for (GList *link = r_decals.allocated->head; link;) {
    r_decal_t *decal = link->data;
    GList *next = link->next;

    if (decal->lifetime > 0) {
      const uint32_t age = view->ticks - decal->time;
      if (age >= decal->lifetime) {
        R_FreeDecal(decal);
        link = next;
        continue;
      }
    }
    link = next;
  }

  Com_Debug(DEBUG_RENDERER, "Persistent decals: %u\n", g_queue_get_length(r_decals.allocated));

  // Add active decals to the view
  for (GList *link = r_decals.allocated->head; link; link = link->next) {
    r_decal_t *decal = link->data;
    
    if (view->num_decals < MAX_DECALS) {
      // Apply fade effect near end of life
      r_decal_t d = *decal;
      if (d.lifetime > 0) {
        const uint32_t age = view->ticks - d.time;
        const uint32_t fade_duration = d.lifetime / 5; // Fade during last 20% of life
        const uint32_t fade_start = d.lifetime - fade_duration;
        if (age > fade_start) {
          const float fade = 1.0 - ((float) (age - fade_start) / (float) fade_duration);
          d.color.a *= fade;
        }
      }
      view->decals[view->num_decals++] = d;
    }
  }

  // Regenerate decal geometry
  r_decals.num_vertexes = 0;
  r_decals.num_elements = 0;

  if (r_models.world && r_models.world->bsp) {
    const r_decal_t *decal = view->decals;
    for (int32_t i = 0; i < view->num_decals; i++, decal++) {
      R_DecalNode(decal, r_models.world->bsp->nodes);

      // Also check BSP inline models (func_* entities)
      const r_entity_t *e = view->entities;
      for (int32_t j = 0; j < view->num_entities; j++, e++) {
        if (e->model && e->model->type == MODEL_BSP_INLINE) {
          r_decal_t inline_decal = *decal;
          inline_decal.origin = Mat4_Transform(e->inverse_matrix, decal->origin);
          // Transform normal (direction) - w=0 means no translation
          inline_decal.normal = Mat4_Transform(e->inverse_matrix, Vec3_Add(decal->origin, decal->normal));
          inline_decal.normal = Vec3_Normalize(Vec3_Subtract(inline_decal.normal, inline_decal.origin));
          R_DecalNode(&inline_decal, e->model->bsp_inline->head_node);
        }
      }
    }
  }

  // Upload geometry to GPU
  if (r_decals.num_vertexes > 0) {
    Com_Debug(DEBUG_RENDERER, "Uploading %d decal vertexes, %d elements\n", r_decals.num_vertexes, r_decals.num_elements);
    glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, r_decals.num_vertexes * sizeof(r_decal_vertex_t), r_decals.vertexes);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, r_decals.num_elements * sizeof(GLuint), r_decals.elements);
  }

  R_GetError(NULL);
}

/**
 * @brief Draw decals.
 */
void R_DrawDecals(const r_view_t *view) {

  if (r_decals.num_elements == 0) {
    return;
  }

  Com_Debug(DEBUG_RENDERER, "Drawing %d decal elements (%d vertexes)\n", r_decals.num_elements, r_decals.num_vertexes);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1.0, -1.0);

  glUseProgram(r_decal_program.name);

  glBindVertexArray(r_decals.vertex_array);
  
  // Bind texture from first persistent decal (they should all share atlas for now)
  // TODO: Batch by texture for better performance
  if (g_queue_get_length(r_decals.allocated) > 0) {
    r_decal_t *first = g_queue_peek_head(r_decals.allocated);
    if (first && first->media) {
      const r_image_t *image = (const r_image_t *) first->media;
      glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);
      glBindTexture(GL_TEXTURE_2D, image->texnum);
      Com_Debug(DEBUG_RENDERER, "Binding decal texture %u\n", image->texnum);
    }
  } else {
    Com_Debug(DEBUG_RENDERER, "No decals to bind texture for\n");
  }

  glDrawElements(GL_TRIANGLES, r_decals.num_elements, GL_UNSIGNED_INT, NULL);

  glUseProgram(0);

  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  R_GetError(NULL);
}

/**
 * @brief Initialize the decals subsystem.
 */
void R_InitDecals(void) {

  memset(&r_decals, 0, sizeof(r_decals));

  r_decals.allocated = g_queue_new();
  r_decals.free = g_queue_new();

  // Load shader program
  r_decal_program.name = R_LoadProgram(
    R_ShaderDescriptor(GL_VERTEX_SHADER, "decal_vs.glsl", NULL),
    R_ShaderDescriptor(GL_FRAGMENT_SHADER, "decal_fs.glsl", NULL),
    NULL);

  glUseProgram(r_decal_program.name);

  r_decal_program.uniforms_block = glGetUniformBlockIndex(r_decal_program.name, "uniforms_block");
  glUniformBlockBinding(r_decal_program.name, r_decal_program.uniforms_block, 0);

  r_decal_program.texture_diffusemap = glGetUniformLocation(r_decal_program.name, "texture_diffusemap");
  glUniform1i(r_decal_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);

  glUseProgram(0);

  // Create vertex buffer
  glGenBuffers(1, &r_decals.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(r_decals.vertexes), NULL, GL_DYNAMIC_DRAW);

  // Create element buffer
  glGenBuffers(1, &r_decals.elements_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(r_decals.elements), NULL, GL_DYNAMIC_DRAW);

  // Create vertex array object
  glGenVertexArrays(1, &r_decals.vertex_array);
  glBindVertexArray(r_decals.vertex_array);

  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);

  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_decal_vertex_t), (void *)offsetof(r_decal_vertex_t, position));

  // Texcoord attribute
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_decal_vertex_t), (void *)offsetof(r_decal_vertex_t, texcoord));

  // Color attribute
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_decal_vertex_t), (void *)offsetof(r_decal_vertex_t, color));

  glBindVertexArray(0);

  R_GetError("R_InitDecals");
}

/**
 * @brief Shutdown the decals subsystem.
 */
void R_ShutdownDecals(void) {

  if (r_decal_program.name) {
    glDeleteProgram(r_decal_program.name);
  }

  if (r_decals.vertex_array) {
    glDeleteVertexArrays(1, &r_decals.vertex_array);
  }

  if (r_decals.vertex_buffer) {
    glDeleteBuffers(1, &r_decals.vertex_buffer);
  }

  if (r_decals.elements_buffer) {
    glDeleteBuffers(1, &r_decals.elements_buffer);
  }

  memset(&r_decals, 0, sizeof(r_decals));
}
