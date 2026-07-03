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

  int32_t char_width;
  int32_t char_height;
} r_font_t;

#define MAX_DRAW_FONTS 3

typedef struct {
  int32_t x, y, w, h;
} r_draw_2d_clipping_frame_t;

/**
 * @brief A batch of 2D primitives sharing a texture, primitive type and clip.
 */
typedef struct {
  SDL_GPUPrimitiveType mode;
  const Texture *texture;

  r_draw_2d_clipping_frame_t clipping_frame;

  int32_t first_vertex;
  int32_t num_vertexes;
} r_draw_2d_arrays_t;

#define MAX_DRAW_2D_ARRAYS 8192

/**
 * @brief 2D vertex struct.
 */
typedef struct {
  vec2_t position;
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

  // the triangle-list and line-strip pipelines
  GraphicsPipeline *pipeline_triangles;
  GraphicsPipeline *pipeline_lines;

  // the diffuse sampler
  Sampler *sampler;

  // the dynamic vertex buffer and its capacity, in vertexes
  Buffer *vertex_buffer;
  uint32_t vertex_buffer_capacity;

} r_draw_2d;

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

  // Triangle lists concatenate; merge with the previous compatible batch.
  if (list->num_draw_arrays && draw->mode == SDL_GPU_PRIMITIVETYPE_TRIANGLELIST) {
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
 * @brief Emits two triangles from the specified quad.
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
static void R_Draw2DChar_(int32_t x, int32_t y, char c, const color_t color) {

  if (isspace(c) && c != 0x0b) {
    return;
  }

  const uint32_t row = (uint32_t) c >> 4;
  const uint32_t col = (uint32_t) c & 15;

  const float s0 = col * 0.0625;
  const float t0 = row * 0.1250;
  const float s1 = (col + 1) * 0.0625;
  const float t1 = (row + 1) * 0.1250;

  const int32_t cw = r_draw_2d.font->char_width;
  const int32_t ch = r_draw_2d.font->char_height;

  r_draw_2d_vertex_t quad[4];

  quad[0].position = Vec2(x, y);
  quad[1].position = Vec2(x + cw, y);
  quad[2].position = Vec2(x + cw, y + ch);
  quad[3].position = Vec2(x, y + ch);

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
void R_Draw2DChar(int32_t x, int32_t y, char c, const color_t color) {

  if (isspace(c) && c != 0x0b) {
    return;
  }

  r_draw_2d_arrays_t draw = {
    .mode = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .texture = r_draw_2d.font->image->texture,
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
int32_t R_StringWidth(const char *s) {

  size_t len = 0;

  while (*s) {
    if (q_striscolor(s)) {
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

  return (int32_t) (len * r_draw_2d.font->char_width);
}

/**
 * @brief Draws a null-terminated string at the specified screen position.
 */
size_t R_Draw2DString(int32_t x, int32_t y, const char *s, const color_t color) {
  return R_Draw2DSizedString(x, y, s, UINT16_MAX, UINT16_MAX, color);
}

/**
 * @brief Draws up to `size` bytes of a string at the specified screen position.
 */
size_t R_Draw2DBytes(int32_t x, int32_t y, const char *s, size_t size, const color_t color) {
  return R_Draw2DSizedString(x, y, s, size, size, color);
}

/**
 * @brief Draws at most len chars or size bytes of the specified string. Color escape
 * sequences are not visible chars. Returns the number of chars drawn.
 */
size_t R_Draw2DSizedString(int32_t x, int32_t y, const char *s, size_t len, size_t size, const color_t color) {
  size_t i, j;

  r_draw_2d_arrays_t draw = {
    .mode = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .texture = r_draw_2d.font->image->texture,
    .first_vertex = r_draw_2d.active->num_vertexes
  };

  color_t c = color;

  i = j = 0;
  while (*s && i < len && j < size) {

    if (q_striscolor(s)) {
      c = ColorEsc(q_strcolor(s));
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
      q_snprintf(path, sizeof(path), "pics/emoji/%s", name);

      const r_image_t *emoji = R_LoadImage(path, IMG_PIC) ?: r_draw_2d.null_texture;

      R_Draw2DImage(x, y, r_draw_2d.font->char_height, r_draw_2d.font->char_height, emoji, color_white);
      x += r_draw_2d.font->char_height;

      i += 2;
      j += q_strlen(name) + 2;

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
void R_BindFont(const char *name, int32_t *cw, int32_t *ch) {

  if (name == NULL) {
    name = "medium";
  }

  const bool upscale = r_context.display_mode->pixel_density > 1.f;

  int32_t i;
  for (i = 0; i < r_draw_2d.num_fonts; i++) {
    if (!q_strcmp(name, r_draw_2d.fonts[i].name)) {
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
void R_SetClippingFrame(int32_t x, int32_t y, int32_t w, int32_t h) {

  r_draw_2d.clipping_frame.x = x;
  r_draw_2d.clipping_frame.y = y;
  r_draw_2d.clipping_frame.w = w;
  r_draw_2d.clipping_frame.h = h;
}

/**
 * @brief Draws a 2D image or atlas image at the specified screen rectangle.
 */
void R_Draw2DImage(int32_t x, int32_t y, int32_t w, int32_t h, const r_image_t *image, const color_t color) {

  if (image == NULL) {
    Com_Warn("NULL image\n");
    return;
  }

  r_draw_2d_arrays_t draw = {
    .mode = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .texture = image->texture,
    .first_vertex = r_draw_2d.active->num_vertexes,
    .num_vertexes = 6
  };

  r_draw_2d_vertex_t quad[4];

  quad[0].position = Vec2(x, y);
  quad[1].position = Vec2(x + w, y);
  quad[2].position = Vec2(x + w, y + h);
  quad[3].position = Vec2(x, y + h);

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
 * @brief Draws a framebuffer's color attachment as a 2D image at the specified screen rectangle.
 * @remarks TODO(#864): used to composite the player-model view; wire it up (sample
 * framebuffer->colorTextures[0]) when R_DrawPlayerModelView is ported.
 */
void R_Draw2DFramebuffer(int32_t x, int32_t y, int32_t w, int32_t h, const Framebuffer *framebuffer, const color_t color) {
}

/**
 * @brief The color can be specified as an index into the palette with positive alpha
 * value for a, or as an RGBA value (32 bit) by passing -1.0 for a.
 */
void R_Draw2DFill(int32_t x, int32_t y, int32_t w, int32_t h, const color_t color) {

  r_draw_2d_arrays_t draw = {
    .mode = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .texture = r_draw_2d.null_texture->texture,
    .first_vertex = r_draw_2d.active->num_vertexes,
    .num_vertexes = 6
  };

  r_draw_2d_vertex_t quad[4];

  quad[0].position = Vec2(x, y);
  quad[1].position = Vec2(x + w, y);
  quad[2].position = Vec2(x + w, y + h);
  quad[3].position = Vec2(x, y + h);

  quad[0].diffusemap = Vec2(0.f, 0.f);
  quad[1].diffusemap = Vec2(1.f, 0.f);
  quad[2].diffusemap = Vec2(1.f, 1.f);
  quad[3].diffusemap = Vec2(0.f, 1.f);

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
void R_Draw2DLines(const int32_t *points, size_t count, const color_t color) {

  r_draw_2d_arrays_t draw = {
    .mode = SDL_GPU_PRIMITIVETYPE_LINESTRIP,
    .texture = r_draw_2d.null_texture->texture,
    .first_vertex = r_draw_2d.active->num_vertexes,
    .num_vertexes = (int32_t) count
  };

  r_draw_2d_arrays_list_t *list = r_draw_2d.active;

  if (list->num_vertexes + (int32_t) count > MAX_DRAW_2D_VERTEXES) {
    Com_Warn("R_Draw2DLines: vertex buffer overflow; truncating %zu vertices\n", count);
    count = (size_t) Maxi(MAX_DRAW_2D_VERTEXES - list->num_vertexes, 0);
    if (count == 0) {
      return;
    }
    draw.num_vertexes = (int32_t) count;
  }

  r_draw_2d_vertex_t *out = list->vertexes + list->num_vertexes;

  const int32_t *in = points;
  for (size_t i = 0; i < count; i++, in += 2, out++) {

    out->position.x = *(in + 0);
    out->position.y = *(in + 1);
    out->diffusemap = Vec2(0.f, 0.f);

    out->color = Color_Color32(color);
  }

  list->num_vertexes += count;

  R_AddDraw2DArrays(&draw);

  r_stats.draw_lines += count >> 1;
}

/**
 * @brief Records the batches for a single draw list under the given projection.
 * @param offset The base vertex offset of this list within the shared vertex buffer.
 */
static void R_Draw2DList(RenderPass *pass, const r_draw_2d_arrays_list_t *list,
                         const mat4_t projection, int32_t offset) {

  CommandBuffer *commands = r_context.device->commands;
  const Framebuffer *framebuffer = r_context.device->framebuffer;

  $(commands, pushVertexUniformData, 0, projection.array, sizeof(projection));

  const GraphicsPipeline *bound = NULL;

  const r_draw_2d_arrays_t *d = list->draw_arrays;
  for (int32_t i = 0; i < list->num_draw_arrays; i++, d++) {

    if (!d->texture) {
      continue;
    }

    GraphicsPipeline *pipeline = d->mode == SDL_GPU_PRIMITIVETYPE_LINESTRIP
        ? r_draw_2d.pipeline_lines : r_draw_2d.pipeline_triangles;

    if (pipeline != bound) {
      $(pass, bindPipeline, pipeline);
      bound = pipeline;
    }

    const r_draw_2d_clipping_frame_t *c = &d->clipping_frame;
    if (c->w || c->h) {
      $(pass, setScissor, &(SDL_Rect) { c->x, c->y, c->w, c->h });
    } else {
      $(pass, setScissor, &(SDL_Rect) { 0, 0, framebuffer->size.w, framebuffer->size.h });
    }

    $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
      .texture = d->texture->texture,
      .sampler = r_draw_2d.sampler->sampler,
    }, 1);

    $(pass, drawPrimitives, d->num_vertexes, 1, d->first_vertex + offset, 0);
  }
}

/**
 * @brief Draw all 2D geometry accumulated for the current frame.
 */
void R_Draw2D(void) {

  r_stats.draw_arrays = r_draw_2d.game.num_draw_arrays + r_draw_2d.ui.num_draw_arrays;

  if (r_stats.draw_arrays == 0) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands || !r_draw_2d.pipeline_triangles) {
    goto reset;
  }

  Framebuffer *framebuffer = r_context.device->framebuffer;

  const uint32_t game_count = (uint32_t) r_draw_2d.game.num_vertexes;
  const uint32_t ui_count = (uint32_t) r_draw_2d.ui.num_vertexes;
  const uint32_t total = game_count + ui_count;

  if (total > r_draw_2d.vertex_buffer_capacity) {
    r_draw_2d.vertex_buffer = release(r_draw_2d.vertex_buffer);
    r_draw_2d.vertex_buffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = total * sizeof(r_draw_2d_vertex_t),
    });
    r_draw_2d.vertex_buffer_capacity = total;
  }

  {
    CopyPass *copyPass = $(commands, beginCopyPass);

    if (game_count) {
      $(copyPass, uploadData, r_draw_2d.vertex_buffer->buffer, r_draw_2d.game.vertexes,
        game_count * sizeof(r_draw_2d_vertex_t), 0, true);
    }
    if (ui_count) {
      $(copyPass, uploadData, r_draw_2d.vertex_buffer->buffer, r_draw_2d.ui.vertexes,
        ui_count * sizeof(r_draw_2d_vertex_t), game_count * sizeof(r_draw_2d_vertex_t), game_count == 0);
    }

    release(copyPass);
  }

  const SDL_GPUColorTargetInfo color =
      $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, NULL);

  RenderPass *pass = $(commands, beginRenderPass, &color, 1, NULL);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = r_draw_2d.vertex_buffer->buffer }, 1);

  // Game elements (console, HUD) use r_context pixel coordinates.
  const mat4_t game_projection = Mat4_FromOrtho(0.f, r_context.w, r_context.h, 0.f, -1.f, 1.f);
  R_Draw2DList(pass, &r_draw_2d.game, game_projection, 0);

  // UI elements (menus) use raw window coordinates.
  const mat4_t ui_projection = Mat4_FromOrtho(0.f, r_context.window_bounds.w, r_context.window_bounds.h, 0.f, -1.f, 1.f);
  R_Draw2DList(pass, &r_draw_2d.ui, ui_projection, (int32_t) game_count);

  release(pass);

reset:
  r_draw_2d.game.num_vertexes = 0;
  r_draw_2d.game.num_draw_arrays = 0;

  r_draw_2d.ui.num_vertexes = 0;
  r_draw_2d.ui.num_draw_arrays = 0;
}

/**
 * @brief Initializes the specified bitmap font. The layout of the font is square,
 * 2^n (e.g. 256x256, 512x512), and 8 rows by 16 columns.
 */
static void R_InitFont(char *name) {

  if (r_draw_2d.num_fonts == MAX_DRAW_FONTS) {
    Com_Error(ERROR_DROP, "MAX_DRAW_FONTS\n");
  }

  r_font_t *font = &r_draw_2d.fonts[r_draw_2d.num_fonts++];

  q_strlcpy(font->name, name, sizeof(font->name));

  font->image = R_LoadImage(va("ui/fonts/%s", name), IMG_FONT);
  assert(font->image);

  const float scale = SDL_GetWindowDisplayScale(r_context.window);

  font->char_width = font->image->width / scale / 16.f;
  font->char_height = font->image->height / scale / 8.f;

  Com_Debug(DEBUG_RENDERER, "%s (%dx%d)\n", font->name, font->char_width, font->char_height);
}

/**
 * @brief Builds a 2D pipeline (draw_2d_vs/draw_2d_fs) for the given primitive type.
 */
static GraphicsPipeline *R_InitDraw2DPipeline(Shader *vertexShader, Shader *fragmentShader, SDL_GPUPrimitiveType mode) {

  const Framebuffer *framebuffer = r_context.device->framebuffer;

  SDL_GPUGraphicsPipelineCreateInfo info = {
    .vertex_shader = vertexShader->shader,
    .fragment_shader = fragmentShader->shader,
    .primitive_type = mode,
    .vertex_input_state = {
      .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
        .slot = 0,
        .pitch = sizeof(r_draw_2d_vertex_t),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      },
      .num_vertex_buffers = 1,
      .vertex_attributes = (SDL_GPUVertexAttribute[]) {
        {
          .location = 0,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(r_draw_2d_vertex_t, position),
        },
        {
          .location = 1,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(r_draw_2d_vertex_t, diffusemap),
        },
        {
          .location = 2,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
          .offset = offsetof(r_draw_2d_vertex_t, color),
        },
      },
      .num_vertex_attributes = 3,
    },
    .rasterizer_state = {
      .fill_mode = SDL_GPU_FILLMODE_FILL,
      .cull_mode = SDL_GPU_CULLMODE_NONE,
      .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
    },
    .target_info = {
      .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
        .format = framebuffer->colorFormats[0],
        .blend_state = GPU_BlendStateAlpha,
      },
      .num_color_targets = 1,
    },
  };

  return $(r_context.device, createGraphicsPipeline, &info);
}

/**
 * @brief Initializes the 2D draw subsystem, loading fonts and creating GPU resources.
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
  r_draw_2d.null_texture->texture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1,
    .height = 1,
    .layer_count_or_depth = 1,
    .num_levels = 1,
  }, &(const uint32_t) { 0xffffffff });
  R_RegisterMedia((r_media_t *) r_draw_2d.null_texture);

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/draw_2d_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 1, // projection2D (binding 0)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/draw_2d_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 1, // texture_diffusemap
  });

  r_draw_2d.pipeline_triangles = R_InitDraw2DPipeline(vertexShader, fragmentShader, SDL_GPU_PRIMITIVETYPE_TRIANGLELIST);
  r_draw_2d.pipeline_lines = R_InitDraw2DPipeline(vertexShader, fragmentShader, SDL_GPU_PRIMITIVETYPE_LINESTRIP);

  release(vertexShader);
  release(fragmentShader);

  r_draw_2d.sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });

  R_SetDraw2DProjection(PROJECTION_GAME);
}

/**
 * @brief Shuts down the 2D draw subsystem, freeing GPU resources.
 */
void R_ShutdownDraw2D(void) {

  r_draw_2d.pipeline_triangles = release(r_draw_2d.pipeline_triangles);
  r_draw_2d.pipeline_lines = release(r_draw_2d.pipeline_lines);
  r_draw_2d.sampler = release(r_draw_2d.sampler);
  r_draw_2d.vertex_buffer = release(r_draw_2d.vertex_buffer);
}
