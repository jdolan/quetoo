// Standalone validation harness mirroring Quetoo's r_vk.c device-selection logic
// (R_Vk_ScoreDevice / R_Vk_SelectPhysicalDevice). Proves the selection algorithm
// picks the right GPU and detects RTX on real hardware. Build: cc vk_probe.c -lvulkan
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#define lengthof(x) (sizeof(x) / sizeof(x[0]))

static const char *rtx_extensions[] = {
  VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
  VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
  VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
  VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};

static int device_has_extensions(VkPhysicalDevice d, const char **names, uint32_t count) {
  uint32_t n = 0;
  vkEnumerateDeviceExtensionProperties(d, NULL, &n, NULL);
  VkExtensionProperties *av = malloc(sizeof(*av) * n);
  vkEnumerateDeviceExtensionProperties(d, NULL, &n, av);
  int all = 1;
  for (uint32_t i = 0; i < count; i++) {
    int found = 0;
    for (uint32_t j = 0; j < n; j++)
      if (!strcmp(av[j].extensionName, names[i])) { found = 1; break; }
    if (!found) { all = 0; break; }
  }
  free(av);
  return all;
}

// Mirrors R_Vk_ScoreDevice (minus surface/present, which needs a window)
static uint32_t score_device(VkPhysicalDevice d) {
  const char *swap[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
  if (!device_has_extensions(d, swap, lengthof(swap))) return 0;
  VkPhysicalDeviceProperties p;
  vkGetPhysicalDeviceProperties(d, &p);
  uint32_t score = 1;
  if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
  if (device_has_extensions(d, rtx_extensions, lengthof(rtx_extensions))) score += 10000;
  return score;
}

int main(void) {
  VkApplicationInfo ai = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "Quetoo", .apiVersion = VK_API_VERSION_1_2 };
  VkInstanceCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &ai };
  VkInstance inst;
  if (vkCreateInstance(&ci, NULL, &inst) != VK_SUCCESS) {
    printf("vkCreateInstance failed\n"); return 1;
  }
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(inst, &count, NULL);
  VkPhysicalDevice *devs = malloc(sizeof(*devs) * count);
  vkEnumeratePhysicalDevices(inst, &count, devs);
  printf("Found %u Vulkan device(s):\n", count);

  uint32_t best = 0;
  VkPhysicalDevice chosen = VK_NULL_HANDLE;
  for (uint32_t i = 0; i < count; i++) {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(devs[i], &p);
    uint32_t s = score_device(devs[i]);
    int rtx = device_has_extensions(devs[i], rtx_extensions, lengthof(rtx_extensions));
    printf("  [%u] %-40s score=%-6u rtx=%s\n", i, p.deviceName, s, rtx ? "yes" : "no");
    if (s > best) { best = s; chosen = devs[i]; }
  }

  if (chosen) {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(chosen, &p);
    int rtx = device_has_extensions(chosen, rtx_extensions, lengthof(rtx_extensions));
    printf("\nSELECTED: %s\n", p.deviceName);
    printf("Ray tracing (RTX): %s\n", rtx ? "available" : "not available");
  }
  free(devs);
  vkDestroyInstance(inst, NULL);
  return 0;
}
