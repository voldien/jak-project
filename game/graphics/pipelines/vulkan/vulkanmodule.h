
#pragma once

#define GLFW_INCLUDE_NONE
#include <mutex>

#include "VKHelper.h"
#include "game/graphics/pipelines/glfw_display.h"

#include "common/dma/dma_copy.h"
#include "common/global_profiler/GlobalProfiler.h"
#include "common/goal_constants.h"
#include "common/log/log.h"
#include "common/util/FileUtil.h"
#include "common/util/FrameLimiter.h"
#include "common/util/Timer.h"
#include "common/util/compress.h"

#include "game/graphics/display.h"
#include "game/graphics/gfx.h"
#include "game/graphics/opengl_renderer/debug_gui.h"
#include "game/graphics/texture/TexturePool.h"
#include "game/graphics/vulkan_renderer/vulkanrenderer.h"
#include "game/runtime.h"
#include "game/sce/libscf.h"
#include "game/system/newpad.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

#include "third-party/glad/include/glad/glad.h"
#include "third-party/glfw/include/GLFW/glfw3.h"

typedef struct vulkan_core_t {
  VkInstance instance;

  typedef struct vk_physical_device {
    VkPhysicalDevice physicalDevices;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memProperties;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceLimits limits;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    std::vector<VkExtensionProperties> extensions;
  } VkPhysicaldevice;

  std::vector<VkPhysicaldevice> physicalDev;
  VkPhysicalDevice selectPhysicalDevice;

  VkPhysicaldevice* selectPhysicalDev;

} VulkanCore;
constexpr PerGameVersion<int> fr3_level_count(jak1::LEVEL_TOTAL, jak2::LEVEL_TOTAL);

struct GraphicsData {
  // vsync
  std::mutex sync_mutex;
  std::condition_variable sync_cv;

  VulkanCore vulkanCore;

  // dma chain transfer
  std::mutex dma_mutex;
  std::condition_variable dma_cv;
  u64 frame_idx = 0;
  u64 frame_idx_of_input_data = 0;
  bool has_data_to_render = false;
  FixedChunkDmaCopier dma_copier;

  // texture pool
  std::shared_ptr<TexturePool> texture_pool;

  std::shared_ptr<Loader> loader;

  // temporary opengl renderer
  VulkanRenderer ogl_renderer;
  //
  OpenGlDebugGui debug_gui;

  FrameLimiter frame_limiter;
  Timer engine_timer;
  double last_engine_time = 1. / 60.;
  float pmode_alp = 0.f;

  std::string imgui_log_filename, imgui_filename;
  GameVersion version;

  struct {
    bool callbacks_registered = false;
    GLFWmonitor** monitors;
    int monitor_count;
  } g_glfw_state;

  GraphicsData(GameVersion version)
      : dma_copier(EE_MAIN_MEM_SIZE),
        texture_pool(std::make_shared<TexturePool>(version)),
        loader(std::make_shared<Loader>(
            file_util::get_jak_project_dir() / "out" / game_version_names[version] / "fr3",
            fr3_level_count[version])),
        ogl_renderer(texture_pool, loader, version),
        version(version) {}
};

extern std::unique_ptr<GraphicsData> g_gfx_data;

// void SetGlobalGLFWCallbacks();

// void ClearGlobalGLFWCallbacks();
void vk_exit();

u32 vk_vsync();

u32 vk_sync_path();
void vk_send_chain(const void* data, u32 offset);
void vk_texture_upload_now(const u8* tpage, int mode, u32 s7_ptr);

void vk_texture_relocate(u32 destination, u32 source, u32 format);

void vk_poll_events();

void vk_set_levels(const std::vector<std::string>& levels);

void vk_set_pmode_alp(float val);

extern const GfxRendererModule gRendererVulkan;