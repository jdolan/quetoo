#include "r_local.h"
#include "r_rtx_bridge.h"

#if BUILD_RTX
bool R_Rtx_BridgeInit(r_rtx_bridge_t *bridge, VkExtent2D extent) {
  memset(bridge, 0, sizeof(*bridge));
  bridge->extent = extent;
  bridge->texture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_2D, .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER, .width = extent.width, .height = extent.height,
    .layer_count_or_depth = 1, .num_levels = 1, .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, NULL);
  if (!bridge->texture) {
    return false;
  }

  bridge->transfer = $(r_context.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = extent.width * extent.height * 4,
  });
  if (!bridge->transfer) {
    bridge->texture = release(bridge->texture);
    return false;
  }

  return true;
}

bool R_Rtx_BridgeUpload(r_rtx_bridge_t *bridge, const r_rtx_output_t *output) {
  if (!r_context.device->commands || !bridge->texture || bridge->extent.width != output->extent.width || bridge->extent.height != output->extent.height) return false;
  if (!bridge->transfer) return false;
  const uint32_t size = output->extent.width * output->extent.height * 4;
  void *pixels = $(bridge->transfer, map, true);
  memcpy(pixels, output->pixels, size);
  $(bridge->transfer, unmap);
  CopyPass *copy = $(r_context.device->commands, beginCopyPass);
  $(copy, uploadTexture, &(SDL_GPUTextureTransferInfo) { .transfer_buffer = bridge->transfer->buffer, .pixels_per_row = output->extent.width, .rows_per_layer = output->extent.height }, &(SDL_GPUTextureRegion) { .texture = bridge->texture->texture, .w = output->extent.width, .h = output->extent.height, .d = 1 }, false);
  release(copy);
  return true;
}

void R_Rtx_BridgeDraw(const r_rtx_bridge_t *bridge, int32_t x, int32_t y,
                      int32_t w, int32_t h, color_t color) {
  r_image_t image = { .texture = bridge->texture };
  R_Draw2DImage(x, y, w, h, &image, color);
}

void R_Rtx_BridgeShutdown(r_rtx_bridge_t *bridge) {
  bridge->transfer = release(bridge->transfer);
  bridge->texture = release(bridge->texture);
  memset(bridge, 0, sizeof(*bridge));
}
#endif
