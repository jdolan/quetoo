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

/**
 * @brief The font type.
 */
typedef struct {
  char name[MAX_QPATH];

  r_image_t *image;

  GLint char_width;
  GLint char_height;
} r_font_t;

#define MAX_DRAW_FONTS 3

typedef struct {
  GLint x, y, w, h;
} r_draw_2d_clipping_frame_t;

/**
 * @brief glDrawArrays commands.
 */
typedef struct {
  GLenum mode;
  GLuint texture;

  r_draw_2d_clipping_frame_t clipping_frame;

  GLint first_vertex;
  GLsizei num_vertexes;
} r_draw_2d_arrays_t;

#define MAX_DRAW_2D_ARRAYS 8192

/**
 * @brief 2D vertex struct.
 */
typedef struct {
  vec2s_t position;
  vec2_t diffusemap;
  color32_t color;
} r_draw_2d_vertex_t;

#define MAX_DRAW_2D_VERTEXES (MAX_DRAW_2D_ARRAYS * 6)

/**
 * @brief A list of 2D draw calls and their backing vertexes.
 */
typedef struct {
  r_draw_2d_arrays_t draw_arrays[MAX_DRAW_2D_ARRAYS];
  int32_t num_draw_arrays;

  r_draw_2d_vertex_t vertexes[MAX_DRAW_2D_VERTEXES];
  int32_t num_vertexes;
} r_draw_2d_arrays_list_t;

/**
 * @brief 2D draw struct.
 */
static struct {

  // registered fonts
  int32_t num_fonts;
  r_font_t fonts[MAX_DRAW_FONTS];

  // active font
  r_font_t *font;

  // active clipping frame, copied to each command
  r_draw_2d_clipping_frame_t clipping_frame;

  // the null texture
  r_image_t *null_texture;

  // draw list for ui elements like menus, using raw window coordinates
  r_draw_2d_arrays_list_t ui;

  // draw list for in-game elements like console, hud, using r_draw_scale window coordinates
  r_draw_2d_arrays_list_t game;

  // active draw arrays list
  r_draw_2d_arrays_list_t *active;

  // the vertex array
  GLuint vertex_array;

  // the vertex buffer
  GLuint vertex_buffer;

} r_draw_2d;

/**
 * @brief The draw program.
 */
static struct {
  GLuint name;

  GLuint uniforms_block;

  GLint projection2D;

  GLint texture_diffusemap;
} r_draw_2d_program;

/**
 * @brief Sets the active 2D projection context (game or UI).
 */
void R_SetDraw2DProjection(r_draw_2d_projection_t projection) {

  switch (projection) {
    case PROJECTION_GAME:
      r_draw_2d.active = &r_draw_2d.game;
      break;
    case PROJECTION_UI:
      r_draw_2d.active = &r_draw_2d.ui;
      break;
  }
}

/**
 * @brief Appends a draw arrays batch to the active 2D draw list, merging adjacent compatible draws.
 */
static void R_AddDraw2DArrays(const r_draw_2d_arrays_t *draw) {

  r_draw_2d_arrays_list_t *list = r_draw_2d.active;

  if (list->num_draw_arrays == MAX_DRAW_2D_ARRAYS) {
    Com_Warn("MAX_DRAW_2D_ARRAYS\n");
    return;
  }

  if (draw->num_vertexes == 0) {
    return;
  }

  if (list->num_draw_arrays && (draw->mode == GL_LINES || draw->mode == GL_TRIANGLES || draw->mode == GL_POINTS)) {
    r_draw_2d_arrays_t *last_draw = &list->draw_arrays[list->num_draw_arrays - 1];
    const r_draw_2d_clipping_frame_t *f = &last_draw->clipping_frame;
    if (last_draw->mode == draw->mode
        && last_draw->texture == draw->texture
        && memcmp(f, &r_draw_2d.clipping_frame, sizeof(*f)) == 0) {
      last_draw->num_vertexes += draw->num_vertexes;
      return;
    }
  }

  r_draw_2d_arrays_t *out = &list->draw_arrays[list->num_draw_arrays];

  *out = *draw;

  out->clipping_frame = r_draw_2d.clipping_frame;

  list->num_draw_arrays++;
}

/**
 * @brief Emits `GL_TRIANGLES` data from the specified quad.
 */
static void R_EmitDrawVertexes2D_Quad(const r_draw_2d_vertex_t *quad) {

  r_draw_2d_arrays_list_t *list = r_draw_2d.active;

  if (list->num_vertexes + 6 > MAX_DRAW_2D_VERTEXES) {
    Com_Warn("MAX_DRAW_2D_VERTEXES\n");
    return;
  }

  list->vertexes[list->num_vertexes++] = quad[0];
  list->vertexes[list->num_vertexes++] = quad[1];
  list->vertexes[list->num_vertexes++] = quad[2];

  list->vertexes[list->num_vertexes++] = quad[0];
  list->vertexes[list->num_vertexes++] = quad[2];
  list->vertexes[list->num_vertexes++] = quad[3];
}

/**
 * @brief Emits quad vertices for the given character at the specified screen position.
 */
static void R_Draw2DChar_(GLint x, GLint y, char c, const color_t color) {

  if (isspace(c) && c != 0x0b) {
    return;
  }

  const uint32_t row = (uint32_t) c >> 4;
  const uint32_t col = (uint32_t) c & 15;

  const float s0 = col * 0.0625;
  const float t0 = row * 0.1250;
  const float s1 = (col + 1) * 0.0625;
  const float t1 = (row + 1) * 0.1250;

  const GLint cw = r_draw_2d.font->char_width;
  const GLint ch = r_draw_2d.font->char_height;

  r_draw_2d_vertex_t quad[4];

  quad[0].position = Vec2s(x, y);
  quad[1].position = Vec2s(x + cw, y);
  quad[2].position = Vec2s(x + cw, y + ch);
  quad[3].position = Vec2s(x, y + ch);

  quad[0].diffusemap = Vec2(s0, t0);
  quad[1].diffusemap = Vec2(s1, t0);
  quad[2].diffusemap = Vec2(s1, t1);
  quad[3].diffusemap = Vec2(s0, t1);

  quad[0].color = Color_Color32(color);
  quad[1].color = Color_Color32(color);
  quad[2].color = Color_Color32(color);
  quad[3].color = Color_Color32(color);

  R_EmitDrawVertexes2D_Quad(quad);

  r_stats.draw_chars++;
}

/**
 * @brief Draws a single character at the specified screen position using the current font.
 */
void R_Draw2DChar(GLint x, GLint y, char c, const color_t color) {

  if (isspace(c) && c != 0x0b) {
    return;
  }

  r_draw_2d_arrays_t draw = {
    .mode = GL_TRIANGLES,
    .texture = r_draw_2d.font->image->texnum,
    .first_vertex = r_draw_2d.active->num_vertexes,
    .num_vertexes = 6
  };

  R_Draw2DChar_(x, y, c, color);
  R_AddDraw2DArrays(&draw);
}

/**
 * @brief Return the width of the specified string in pixels. This will vary based
 * on the currently bound font. Color escapes are omitted.
 */
GLint R_StringWidth(const char *s) {

  size_t len = 0;

  while (*s) {
    if (StrIsColor(s)) {
      s += 2;
      continue;
    }

    if (StrIsEmoji(s)) {
      s = EmojiEsc(s, NULL, MAX_STRING_CHARS);
      len += 2;
      continue;
    }

    s++;
    len++;
  }

  return (GLint) (len * r_draw_2d.font->char_width);
}

/**
 * @brief Draws a null-terminated string at the specified screen position.
 */
size_t R_Draw2DString(GLint x, GLint y, const char *s, const color_t color) {
  return R_Draw2DSizedString(x, y, s, UINT16_MAX, UINT16_MAX, color);
}

/**
 * @brief Draws up to `size` bytes of a string at the specified screen position.
 */
size_t R_Draw2DBytes(GLint x, GLint y, const char *s, size_t size, const color_t color) {
  return R_Draw2DSizedString(x, y, s, size, size, color);
}

/**
 * @brief Draws at most len chars or size bytes of the specified string. Color escape
 * sequences are not visible chars. Returns the number of chars drawn.
 */
size_t R_Draw2DSizedString(GLint x, GLint y, const char *s, size_t len, size_t size, const color_t color) {
  size_t i, j;

  r_draw_2d_arrays_t draw = {
    .mode = GL_TRIANGLES,
    .texture = r_draw_2d.font->image->texnum,
    .first_vertex = r_draw_2d.active->num_vertexes
  };

  color_t c = color;

  i = j = 0;
  while (*s && i < len && j < size) {

    if (StrIsColor(s)) {
      c = ColorEsc(StrColor(s));
      j += 2;
      s += 2;
      continue;
    }

    if (StrIsEmoji(s)) {

      draw.num_vertexes = (r_draw_2d.active->num_vertexes) - draw.first_vertex;
      R_AddDraw2DArrays(&draw);

      char name[MAX_QPATH];
      s = EmojiEsc(s, name, sizeof(name));

      char path[MAX_QPATH];
      g_snprintf(path, sizeof(path), "pics/emoji/%s", name);

      const r_image_t *emoji = R_LoadImage(path, IMG_PIC) ?: r_draw_2d.null_texture;

      R_Draw2DImage(x, y, r_draw_2d.font->char_height, r_draw_2d.font->char_height, emoji, color_white);
      x += r_draw_2d.font->char_height;

      i += 2;
      j += strlen(name) + 2;

      draw.first_vertex = r_draw_2d.active->num_vertexes;
      continue;
    }

    R_Draw2DChar_(x, y, *s, c);
    x += r_draw_2d.font->char_width; // next char position in line

    i++;
    j++;
    s++;
  }

  draw.num_vertexes = (r_draw_2d.active->num_vertexes) - draw.first_vertex;
  R_AddDraw2DArrays(&draw);

  return i;
}


/**
 * @brief Binds the specified font, returning the character width and height.
 */
void R_BindFont(const char *name, GLint *cw, GLint *ch) {

  if (name == NULL) {
    name = "medium";
  }

  const bool upscale = r_context.display_mode->pixel_density > 1.f;

  int32_t i;
  for (i = 0; i < r_draw_2d.num_fonts; i++) {
    if (!g_strcmp0(name, r_draw_2d.fonts[i].name)) {
      if (upscale && i < r_draw_2d.num_fonts - 1) {
        r_draw_2d.font = &r_draw_2d.fonts[i + 1];
      } else {
        r_draw_2d.font = &r_draw_2d.fonts[i];
      }
      break;
    }
  }

  if (i == r_draw_2d.num_fonts) {
    Com_Warn("Unknown font: %s\n", name);
  }

  if (cw) {
    *cw = r_draw_2d.font->char_width;
  }

  if (ch) {
    *ch = r_draw_2d.font->char_height;
  }
}

/**
 * @brief Sets the active 2D scissor clipping rectangle.
 */
void R_SetClippingFrame(GLint x, GLint y, GLint w, GLint h) {

  r_draw_2d.clipping_frame.x = x;
  r_draw_2d.clipping_frame.y = y;
  r_draw_2d.clipping_frame.w = w;
  r_draw_2d.clipping_frame.h = h;
}

/**
 * @brief Draws a 2D image or atlas image at the specified screen rectangle.
 */
void R_Draw2DImage(GLint x, GLint y, GLint w, GLint h, const r_image_t *image, const color_t color) {

  if (image == NULL) {
    Com_Warn("NULL image\n");
    return;
  }

  r_draw_2d_arrays_t draw = {
    .mode = GL_TRIANGLES,
    .texture = image->texnum,
    .first_vertex = r_draw_2d.active->num_vertexes,
    .num_vertexes = 6
  };

  r_draw_2d_vertex_t quad[4];

  quad[0].position = Vec2s(x, y);
  quad[1].position = Vec2s(x + w, y);
  quad[2].position = Vec2s(x + w, y + h);
  quad[3].position = Vec2s(x, y + h);

  if (image->media.type == R_MEDIA_ATLAS_IMAGE) {
    const vec4_t st = ((r_atlas_image_t *) image)->texcoords;
    quad[0].diffusemap = Vec2(st.x, st.y);
    quad[1].diffusemap = Vec2(st.z, st.y);
    quad[2].diffusemap = Vec2(st.z, st.w);
    quad[3].diffusemap = Vec2(st.x, st.w);
  } else {
    quad[0].diffusemap = Vec2(0.f, 0.f);
    quad[1].diffusemap = Vec2(1.f, 0.f);
    quad[2].diffusemap = Vec2(1.f, 1.f);
    quad[3].diffusemap = Vec2(0.f, 1.f);
  }

  quad[0].color = Color_Color32(color);
  quad[1].color = Color_Color32(color);
  quad[2].color = Color_Color32(color);
  quad[3].color = Color_Color32(color);

  R_EmitDrawVertexes2D_Quad(quad);
  R_AddDraw2DArrays(&draw);

  r_stats.draw_images++;
}

/**
 * @brief Draws a framebuffer color attachment as a 2D image at the specified screen rectangle.
 */
void R_Draw2DFramebuffer(GLint x, GLint y, GLint w, GLint h, const r_framebuffer_t *framebuffer, const color_t color) {

  if (framebuffer == NULL) {
    Com_Warn("NULL framebuffer\n");
    return;
  }

  r_draw_2d_arrays_t draw = {
    .mode = GL_TRIANGLES,
    .texture = framebuffer->color_attachment,
    .first_vertex = r_draw_2d.active->num_vertexes,
    .num_vertexes = 6
  };

  w = w ?: r_context.w;
  h = h ?: r_context.h;

  r_draw_2d_vertex_t quad[4];

  quad[0].position = Vec2s(x, y);
  quad[1].position = Vec2s(x + w, y);
  quad[2].position = Vec2s(x + w, y + h);
  quad[3].position = Vec2s(x, y + h);

  quad[0].diffusemap = Vec2(0.f, 1.f);
  quad[1].diffusemap = Vec2(1.f, 1.f);
  quad[2].diffusemap = Vec2(1.f, 0.f);
  quad[3].diffusemap = Vec2(0.f, 0.f);

  quad[0].color = Color_Color32(color);
  quad[1].color = Color_Color32(color);
  quad[2].color = Color_Color32(color);
  quad[3].color = Color_Color32(color);

  R_EmitDrawVertexes2D_Quad(quad);
  R_AddDraw2DArrays(&draw);
}

/**
 * @brief The color can be specified as an index into the palette with positive alpha
 * value for a, or as an RGBA value (32 bit) by passing -1.0 for a.
 */
void R_Draw2DFill(GLint x, GLint y, GLint w, GLint h, const color_t color) {

  r_draw_2d_arrays_t draw = {
    .mode = GL_TRIANGLES,
    .texture = r_draw_2d.null_texture->texnum,
    .first_vertex = r_draw_2d.active->num_vertexes,
    .num_vertexes = 6
  };

  r_draw_2d_vertex_t quad[4];

  quad[0].position = Vec2s(x, y);
  quad[1].position = Vec2s(x + w, y);
  quad[2].position = Vec2s(x + w, y + h);
  quad[3].position = Vec2s(x, y + h);

  quad[0].color = Color_Color32(color);
  quad[1].color = Color_Color32(color);
  quad[2].color = Color_Color32(color);
  quad[3].color = Color_Color32(color);

  R_EmitDrawVertexes2D_Quad(quad);
  R_AddDraw2DArrays(&draw);

  r_stats.draw_fills++;
}

/**
 * @brief Draws a polyline through the given screen-space point list.
 */
void R_Draw2DLines(const GLint *points, size_t count, const color_t color) {

  r_draw_2d_arrays_t draw = {
    .mode = GL_LINE_STRIP,
    .texture = r_draw_2d.null_texture->texnum,
    .first_vertex = r_draw_2d.active->num_vertexes,
    .num_vertexes = (GLsizei) count
  };

  r_draw_2d_arrays_list_t *list = r_draw_2d.active;

  if (list->num_vertexes + (int32_t) count > MAX_DRAW_2D_VERTEXES) {
    Com_Warn("R_Draw2DLines: vertex buffer overflow; truncating %zu vertices\n", count);
    count = (size_t) Maxi(MAX_DRAW_2D_VERTEXES - list->num_vertexes, 0);
    if (count == 0) {
      return;
    }
    draw.num_vertexes = (GLsizei) count;
  }

  r_draw_2d_vertex_t *out = list->vertexes + list->num_vertexes;

  const GLint *in = points;
  for (size_t i = 0; i < count; i++, in += 2, out++) {

    out->position.x = *(in + 0);
    out->position.y = *(in + 1);

    out->color = Color_Color32(color);
  }

  list->num_vertexes += count;

  R_AddDraw2DArrays(&draw);

  r_stats.draw_lines += count >> 1;
}

/**
 * @brief Draw all 2D geometry accumulated for the current frame.
 */
void R_Draw2D(void) {

  r_stats.draw_arrays = r_draw_2d.game.num_draw_arrays + r_draw_2d.ui.num_draw_arrays;

  if (r_stats.draw_arrays == 0) {
    return;
  }

  const SDL_Rect viewport = r_context.viewport;
  glViewport(viewport.x, viewport.y, viewport.w, viewport.h);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(r_draw_2d_program.name);
  glBindVertexArray(r_draw_2d.vertex_array);

  const size_t game_size = r_draw_2d.game.num_vertexes * sizeof(r_draw_2d_vertex_t);
  const size_t ui_size = r_draw_2d.ui.num_vertexes * sizeof(r_draw_2d_vertex_t);

  glBindBuffer(GL_ARRAY_BUFFER, r_draw_2d.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, game_size + ui_size, NULL, GL_DYNAMIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, game_size, r_draw_2d.game.vertexes);
  glBufferSubData(GL_ARRAY_BUFFER, game_size, ui_size, r_draw_2d.ui.vertexes);

  {
    const mat4_t projection2D = Mat4_FromOrtho(0.f, r_context.w, r_context.h, 0.f, -1.f, 1.f);
    glUniformMatrix4fv(r_draw_2d_program.projection2D, 1, GL_FALSE, projection2D.array);

    const r_draw_2d_arrays_t *d = r_draw_2d.game.draw_arrays;
    for (int32_t i = 0; i < r_draw_2d.game.num_draw_arrays; i++, d++) {
      const r_draw_2d_clipping_frame_t *c = &d->clipping_frame;
      if (c->w || c->h) {
        glScissor(c->x, c->y, c->w, c->h);
        glEnable(GL_SCISSOR_TEST);
      } else {
        glDisable(GL_SCISSOR_TEST);
      }
      glBindTexture(GL_TEXTURE_2D, d->texture);
      glDrawArrays(d->mode, d->first_vertex, d->num_vertexes);
    }
  }

  {
    const mat4_t projection2D = Mat4_FromOrtho(0.f, r_context.window_bounds.w, r_context.window_bounds.h, 0.f, -1.f, 1.f);
    glUniformMatrix4fv(r_draw_2d_program.projection2D, 1, GL_FALSE, projection2D.array);

    const r_draw_2d_arrays_t *d = r_draw_2d.ui.draw_arrays;
    for (int32_t i = 0; i < r_draw_2d.ui.num_draw_arrays; i++, d++) {
      const r_draw_2d_clipping_frame_t *c = &d->clipping_frame;
      if (c->w || c->h) {
        glScissor(c->x, c->y, c->w, c->h);
        glEnable(GL_SCISSOR_TEST);
      } else {
        glDisable(GL_SCISSOR_TEST);
      }
      glBindTexture(GL_TEXTURE_2D, d->texture);
      glDrawArrays(d->mode, d->first_vertex + r_draw_2d.game.num_vertexes, d->num_vertexes);
    }
  }

  glScissor(viewport.x, viewport.y, viewport.w, viewport.h);
  glDisable(GL_SCISSOR_TEST);

  glBindVertexArray(0);
  glUseProgram(0);

  r_draw_2d.game.num_vertexes = 0;
  r_draw_2d.game.num_draw_arrays = 0;

  r_draw_2d.ui.num_vertexes = 0;
  r_draw_2d.ui.num_draw_arrays = 0;

  glBlendFunc(GL_ONE, GL_ZERO);
  glDisable(GL_BLEND);

  R_GetError(NULL);
}

/**
 * @brief Initializes the specified bitmap font. The layout of the font is square,
 * 2^n (e.g. 256x256, 512x512), and 8 rows by 16 columns. See below:
 *
 *
 *  !"#$%&'()*+,-./
 * 0123456789:;<=>?
 * @ABCDEFGHIJKLMNO
 * `PQRSTUVWXYZ`[\]^_
 * 'abcdefghijklmno
 * pqrstuvwxyz{|}"
 */
static void R_InitFont(char *name) {

  if (r_draw_2d.num_fonts == MAX_DRAW_FONTS) {
    Com_Error(ERROR_DROP, "MAX_DRAW_FONTS\n");
  }

  r_font_t *font = &r_draw_2d.fonts[r_draw_2d.num_fonts++];

  g_strlcpy(font->name, name, sizeof(font->name));

  font->image = R_LoadImage(va("ui/fonts/%s", name), IMG_FONT);
  assert(font->image);
  
  const float scale = SDL_GetWindowDisplayScale(r_context.window);

  font->char_width = font->image->width / scale / 16.f;
  font->char_height = font->image->height / scale / 8.f;

  Com_Debug(DEBUG_RENDERER, "%s (%dx%d)\n", font->name, font->char_width, font->char_height);
}

/**
 * @brief Compiles and links the 2D draw GLSL program, binding texture and projection uniforms.
 */
static void R_InitDraw2DProgram(void) {

  memset(&r_draw_2d_program, 0, sizeof(r_draw_2d_program));

  r_draw_2d_program.name = R_LoadProgram(
      R_ShaderDescriptor(GL_VERTEX_SHADER, "draw_2d_vs.glsl", NULL),
      R_ShaderDescriptor(GL_FRAGMENT_SHADER, "draw_2d_fs.glsl", NULL),
      NULL);

  glUseProgram(r_draw_2d_program.name);

  r_draw_2d_program.uniforms_block = glGetUniformBlockIndex(r_draw_2d_program.name, "uniforms_block");
  glUniformBlockBinding(r_draw_2d_program.name, r_draw_2d_program.uniforms_block, 0);

  r_draw_2d_program.texture_diffusemap = glGetUniformLocation(r_draw_2d_program.name, "texture_diffusemap");
  r_draw_2d_program.projection2D = glGetUniformLocation(r_draw_2d_program.name, "projection2D");

  glUniform1i(r_draw_2d_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);

  glUseProgram(0);

  R_GetError(NULL);
}

#if BUILD_VULKAN

/**
 * @brief Push constants for the Vulkan 2D pipeline; must match vk_2d.vert/.frag.
 */
typedef struct {
  mat4_t projection2D;
  uint32_t texture_index;
} r_vk_2d_push_t;

/**
 * @brief Vulkan 2D draw state: pipelines, the load render pass that composites the
 * UI over the ray-traced frame, and per-frame host-visible vertex buffers.
 */
static struct {
  VkShaderModule vert, frag;
  VkPipelineLayout pipeline_layout;
  VkPipeline triangles;
  VkPipeline lines;
  VkRenderPass render_pass_load;
  VkBuffer vertex_buffers[R_VK_MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory vertex_memory[R_VK_MAX_FRAMES_IN_FLIGHT];
  void *vertex_mapped[R_VK_MAX_FRAMES_IN_FLIGHT];
  VkDeviceSize vertex_capacity;
  _Bool ready;
} r_vk_2d;

static uint32_t R_Vk_2D_FindMemoryType(uint32_t bits, VkMemoryPropertyFlags want) {

  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(r_vk.physical_device, &mp);

  for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
    if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) {
      return i;
    }
  }

  Com_Error(ERROR_FATAL, "No suitable Vulkan memory type\n");
  return 0;
}

static VkShaderModule R_Vk_2D_LoadShader(const char *name) {

  void *buffer;
  const int64_t len = Fs_Load(name, &buffer);
  if (len == -1) {
    Com_Error(ERROR_FATAL, "Failed to load 2D shader %s\n", name);
  }

  const VkShaderModuleCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = (size_t) len,
    .pCode = (const uint32_t *) buffer
  };

  VkShaderModule module;
  if (vkCreateShaderModule(r_vk.device, &ci, NULL, &module) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateShaderModule failed for %s\n", name);
  }

  Fs_Free(buffer);
  return module;
}

/**
 * @brief Creates the render pass used to composite the 2D UI onto the swapchain
 * image after the ray-traced blit: it preserves the existing contents (LOAD) and
 * transitions the image from transfer-dst to present.
 */
static void R_Vk_2D_CreateLoadRenderPass(void) {

  const VkAttachmentDescription color = {
    .format = r_vk.swapchain_format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  };

  const VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

  const VkSubpassDescription subpass = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &ref
  };

  const VkSubpassDependency dep = {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
  };

  const VkRenderPassCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &color,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &dep
  };

  if (vkCreateRenderPass(r_vk.device, &ci, NULL, &r_vk_2d.render_pass_load) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateRenderPass (2D) failed\n");
  }
}

static VkPipeline R_Vk_2D_CreatePipeline(VkPrimitiveTopology topology) {

  const VkPipelineShaderStageCreateInfo stages[] = {
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = r_vk_2d.vert, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = r_vk_2d.frag, .pName = "main" }
  };

  const VkVertexInputBindingDescription binding = { 0, sizeof(r_draw_2d_vertex_t), VK_VERTEX_INPUT_RATE_VERTEX };
  const VkVertexInputAttributeDescription attrs[] = {
    { 0, 0, VK_FORMAT_R16G16_SINT, offsetof(r_draw_2d_vertex_t, position) },
    { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(r_draw_2d_vertex_t, diffusemap) },
    { 2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(r_draw_2d_vertex_t, color) }
  };

  const VkPipelineVertexInputStateCreateInfo vi = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &binding,
    .vertexAttributeDescriptionCount = 3, .pVertexAttributeDescriptions = attrs
  };

  const VkPipelineInputAssemblyStateCreateInfo ia = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = topology
  };

  const VkPipelineViewportStateCreateInfo vp = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1
  };

  const VkPipelineRasterizationStateCreateInfo rs = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.f
  };

  const VkPipelineMultisampleStateCreateInfo ms = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
  };

  const VkPipelineColorBlendAttachmentState cba = {
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
  };

  const VkPipelineColorBlendStateCreateInfo cb = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &cba
  };

  const VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
  const VkPipelineDynamicStateCreateInfo ds = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn
  };

  const VkGraphicsPipelineCreateInfo gpci = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2, .pStages = stages,
    .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vp,
    .pRasterizationState = &rs, .pMultisampleState = &ms, .pColorBlendState = &cb, .pDynamicState = &ds,
    .layout = r_vk_2d.pipeline_layout, .renderPass = r_vk.render_pass, .subpass = 0
  };

  VkPipeline pipeline;
  if (vkCreateGraphicsPipelines(r_vk.device, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateGraphicsPipelines (2D) failed\n");
  }
  return pipeline;
}

/**
 * @brief Initializes the Vulkan 2D pipeline, render pass and per-frame vertex buffers.
 */
void R_Vk_InitDraw2D(void) {

  r_vk_2d.vert = R_Vk_2D_LoadShader("shaders/vk_2d.vert.spv");
  r_vk_2d.frag = R_Vk_2D_LoadShader("shaders/vk_2d.frag.spv");

  const VkDescriptorSetLayout set_layout = R_Vk_TextureSetLayout();
  const VkPushConstantRange pcr = {
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    .offset = 0, .size = sizeof(r_vk_2d_push_t)
  };
  const VkPipelineLayoutCreateInfo plci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1, .pSetLayouts = &set_layout,
    .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr
  };
  if (vkCreatePipelineLayout(r_vk.device, &plci, NULL, &r_vk_2d.pipeline_layout) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreatePipelineLayout (2D) failed\n");
  }

  R_Vk_2D_CreateLoadRenderPass();

  r_vk_2d.triangles = R_Vk_2D_CreatePipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  r_vk_2d.lines = R_Vk_2D_CreatePipeline(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

  r_vk_2d.vertex_capacity = sizeof(r_draw_2d_vertex_t) * MAX_DRAW_2D_VERTEXES * 2;
  for (uint32_t i = 0; i < R_VK_MAX_FRAMES_IN_FLIGHT; i++) {
    const VkBufferCreateInfo bci = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = r_vk_2d.vertex_capacity,
      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(r_vk.device, &bci, NULL, &r_vk_2d.vertex_buffers[i]) != VK_SUCCESS) {
      Com_Error(ERROR_FATAL, "vkCreateBuffer (2D) failed\n");
    }
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(r_vk.device, r_vk_2d.vertex_buffers[i], &req);
    const VkMemoryAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = req.size,
      .memoryTypeIndex = R_Vk_2D_FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (vkAllocateMemory(r_vk.device, &ai, NULL, &r_vk_2d.vertex_memory[i]) != VK_SUCCESS) {
      Com_Error(ERROR_FATAL, "vkAllocateMemory (2D) failed\n");
    }
    vkBindBufferMemory(r_vk.device, r_vk_2d.vertex_buffers[i], r_vk_2d.vertex_memory[i], 0);
    vkMapMemory(r_vk.device, r_vk_2d.vertex_memory[i], 0, r_vk_2d.vertex_capacity, 0, &r_vk_2d.vertex_mapped[i]);
  }

  r_vk_2d.ready = true;
}

/**
 * @brief Records the draw batches for one 2D list with the given projection.
 */
static void R_Vk_2D_DrawList(VkCommandBuffer cb, const r_draw_2d_arrays_list_t *list,
                             mat4_t projection, int32_t vertex_offset, VkDescriptorSet set) {

  VkPipeline bound = VK_NULL_HANDLE;
  const r_draw_2d_arrays_t *d = list->draw_arrays;

  for (int32_t i = 0; i < list->num_draw_arrays; i++, d++) {

    if (d->mode != GL_TRIANGLES && d->mode != GL_LINES) {
      continue; // points and other modes are unused by the UI
    }

    const VkPipeline want = (d->mode == GL_LINES) ? r_vk_2d.lines : r_vk_2d.triangles;
    if (want != bound) {
      vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, want);
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, r_vk_2d.pipeline_layout, 0, 1, &set, 0, NULL);
      bound = want;
    }

    const r_vk_2d_push_t push = { .projection2D = projection, .texture_index = d->texture };
    vkCmdPushConstants(cb, r_vk_2d.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

    vkCmdDraw(cb, d->num_vertexes, 1, d->first_vertex + vertex_offset, 0);
  }
}

/**
 * @brief Records the queued 2D draw lists into the command buffer, compositing the UI
 * onto the swapchain image. `overlay` selects the load render pass (preserving the
 * ray-traced frame underneath); otherwise the clearing render pass is used (menu).
 */
void R_Vk_Draw2D(VkCommandBuffer cb, uint32_t image_index, _Bool overlay) {

  if (!r_vk_2d.ready) {
    return;
  }

  const uint32_t frame = r_vk.current_frame;

  const size_t game_bytes = (size_t) r_draw_2d.game.num_vertexes * sizeof(r_draw_2d_vertex_t);
  const size_t ui_bytes = (size_t) r_draw_2d.ui.num_vertexes * sizeof(r_draw_2d_vertex_t);
  if (game_bytes) {
    memcpy(r_vk_2d.vertex_mapped[frame], r_draw_2d.game.vertexes, game_bytes);
  }
  if (ui_bytes) {
    memcpy((byte *) r_vk_2d.vertex_mapped[frame] + game_bytes, r_draw_2d.ui.vertexes, ui_bytes);
  }

  const VkClearValue clear = { .color = { .float32 = { 0.f, 0.f, 0.f, 1.f } } };
  const VkRenderPassBeginInfo rpbi = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = overlay ? r_vk_2d.render_pass_load : r_vk.render_pass,
    .framebuffer = r_vk.framebuffers[image_index],
    .renderArea = { .offset = { 0, 0 }, .extent = r_vk.swapchain_extent },
    .clearValueCount = 1,
    .pClearValues = &clear
  };
  vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

  if (r_draw_2d.game.num_draw_arrays + r_draw_2d.ui.num_draw_arrays > 0) {

    // negative height flips clip-space Y so the GL-style top-left ortho projection
    // and texture UVs render upright on Vulkan (whose NDC Y points down)
    const VkViewport viewport = {
      0.f, (float) r_vk.swapchain_extent.height,
      (float) r_vk.swapchain_extent.width, -(float) r_vk.swapchain_extent.height,
      0.f, 1.f
    };
    vkCmdSetViewport(cb, 0, 1, &viewport);
    const VkRect2D scissor = { .offset = { 0, 0 }, .extent = r_vk.swapchain_extent };
    vkCmdSetScissor(cb, 0, 1, &scissor);

    const VkDescriptorSet set = R_Vk_TextureSet();
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &r_vk_2d.vertex_buffers[frame], &offset);

    R_Vk_2D_DrawList(cb, &r_draw_2d.game, Mat4_FromOrtho(0.f, r_context.w, r_context.h, 0.f, -1.f, 1.f), 0, set);
    R_Vk_2D_DrawList(cb, &r_draw_2d.ui, Mat4_FromOrtho(0.f, r_context.window_bounds.w, r_context.window_bounds.h, 0.f, -1.f, 1.f), r_draw_2d.game.num_vertexes, set);
  }

  vkCmdEndRenderPass(cb);

  r_draw_2d.game.num_vertexes = 0;
  r_draw_2d.game.num_draw_arrays = 0;
  r_draw_2d.ui.num_vertexes = 0;
  r_draw_2d.ui.num_draw_arrays = 0;
}

/**
 * @brief Destroys the Vulkan 2D pipeline, render pass and buffers.
 */
void R_Vk_ShutdownDraw2D(void) {

  if (!r_vk_2d.ready) {
    return;
  }

  for (uint32_t i = 0; i < R_VK_MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroyBuffer(r_vk.device, r_vk_2d.vertex_buffers[i], NULL);
    vkFreeMemory(r_vk.device, r_vk_2d.vertex_memory[i], NULL);
  }

  vkDestroyPipeline(r_vk.device, r_vk_2d.triangles, NULL);
  vkDestroyPipeline(r_vk.device, r_vk_2d.lines, NULL);
  vkDestroyRenderPass(r_vk.device, r_vk_2d.render_pass_load, NULL);
  vkDestroyPipelineLayout(r_vk.device, r_vk_2d.pipeline_layout, NULL);
  vkDestroyShaderModule(r_vk.device, r_vk_2d.vert, NULL);
  vkDestroyShaderModule(r_vk.device, r_vk_2d.frag, NULL);

  memset(&r_vk_2d, 0, sizeof(r_vk_2d));
}

#endif /* BUILD_VULKAN */

/**
 * @brief Initializes the 2D draw subsystem, loading fonts and creating GPU buffers.
 */
void R_InitDraw2D(void) {

  memset(&r_draw_2d, 0, sizeof(r_draw_2d));

  R_InitFont("small");
  R_InitFont("medium");
  R_InitFont("large");

  R_BindFont(NULL, NULL, NULL);

  r_draw_2d.null_texture = (r_image_t *) R_AllocMedia("null", sizeof(r_image_t), R_MEDIA_IMAGE);
  r_draw_2d.null_texture->media.Retain = R_RetainImage;
  r_draw_2d.null_texture->media.Free = R_FreeImage;
  r_draw_2d.null_texture->type = IMG_PROGRAM;
  r_draw_2d.null_texture->width = 1;
  r_draw_2d.null_texture->height = 1;
  r_draw_2d.null_texture->target = GL_TEXTURE_2D;
  r_draw_2d.null_texture->internal_format = GL_RGBA8;
  r_draw_2d.null_texture->format = GL_RGBA;
  r_draw_2d.null_texture->pixel_type = GL_UNSIGNED_BYTE;
  r_draw_2d.null_texture->magnify = GL_LINEAR;
  r_draw_2d.null_texture->minify = GL_LINEAR;
  r_draw_2d.null_texture->levels = 1;
  R_UploadImage(r_draw_2d.null_texture, &(const uint32_t) { 0xffffffff });

#if BUILD_VULKAN
  if (R_Vulkan()) {
    R_Vk_InitDraw2D();
    R_SetDraw2DProjection(PROJECTION_GAME);
    return;
  }
#endif

  glGenVertexArrays(1, &r_draw_2d.vertex_array);
  glBindVertexArray(r_draw_2d.vertex_array);

  glGenBuffers(1, &r_draw_2d.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, r_draw_2d.vertex_buffer);

  glVertexAttribPointer(0, 2, GL_SHORT, GL_FALSE, sizeof(r_draw_2d_vertex_t), (void *) offsetof(r_draw_2d_vertex_t, position));
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_draw_2d_vertex_t), (void *) offsetof(r_draw_2d_vertex_t, diffusemap));
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_draw_2d_vertex_t), (void *) offsetof(r_draw_2d_vertex_t, color));

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);

  R_GetError(NULL);

  R_InitDraw2DProgram();

  R_SetDraw2DProjection(PROJECTION_GAME);
}

/**
 * @brief Deletes the 2D draw GLSL program object.
 */
static void R_ShutdownDraw2DProgram(void) {

  glDeleteProgram(r_draw_2d_program.name);

  r_draw_2d_program.name = 0;
}

/**
 * @brief Shuts down the 2D draw subsystem, freeing GPU buffers.
 */
void R_ShutdownDraw2D(void) {

#if BUILD_VULKAN
  if (R_Vulkan()) {
    R_Vk_ShutdownDraw2D();
    return;
  }
#endif

  glDeleteVertexArrays(1, &r_draw_2d.vertex_array);
  glDeleteBuffers(1, &r_draw_2d.vertex_buffer);

  R_ShutdownDraw2DProgram();
}
