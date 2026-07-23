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
  return bridge->texture != NULL;
}

bool R_Rtx_BridgeUpload(r_rtx_bridge_t *bridge, const r_rtx_output_t *output) {
  if (!r_context.device->commands || !bridge->texture || bridge->extent.width != output->extent.width || bridge->extent.height != output->extent.height) return false;
  const uint32_t size = output->extent.width * output->extent.height * 4;
  TransferBuffer *transfer = $(r_context.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size });
  void *pixels = $(transfer, map, false);
  memcpy(pixels, output->pixels, size);
  $(transfer, unmap);
  CopyPass *copy = $(r_context.device->commands, beginCopyPass);
  $(copy, uploadTexture, &(SDL_GPUTextureTransferInfo) { .transfer_buffer = transfer->buffer, .pixels_per_row = output->extent.width, .rows_per_layer = output->extent.height }, &(SDL_GPUTextureRegion) { .texture = bridge->texture->texture, .w = output->extent.width, .h = output->extent.height, .d = 1 }, true);
  release(copy);
  release(transfer);
  return true;
}

void R_Rtx_BridgeDraw(const r_rtx_bridge_t *bridge) {
  r_image_t image = { .texture = bridge->texture };
  R_Draw2DImage(0, 0, r_context.w, r_context.h, &image, color_white);
}

void R_Rtx_BridgeShutdown(r_rtx_bridge_t *bridge) { bridge->texture = release(bridge->texture); memset(bridge, 0, sizeof(*bridge)); }
#endif
