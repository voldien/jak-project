
#include "game/graphics/pipelines/vulkan/vulkanmodule.h"

#include "VKHelper.h"

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
#include "game/graphics/opengl_renderer/OpenGLRenderer.h"
#include "game/graphics/opengl_renderer/debug_gui.h"
#include "game/graphics/pipelines/vulkan/vulkandisplay.h"
#include "game/graphics/texture/TexturePool.h"
#include "game/graphics/vulkan_renderer/vulkanrenderer.h"
#include "game/runtime.h"
#include "game/sce/libscf.h"
#include "game/system/newpad.h"

#include "third-party/fmt/core.h"
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_impl_glfw.h"
#include "third-party/imgui/imgui_impl_vulkan.h"

#define STBI_WINDOWS_UTF8
#include "third-party/stb_image/stb_image.h"

using namespace fvkcore;

constexpr bool run_dma_copy = false;

void ErrorCallback(int err, const char* msg) {
  lg::error("GLFW ERR {}: {}", err, std::string(msg));
}

static int vk_init(GfxSettings& settings) {
  if (glfwSetErrorCallback(ErrorCallback) != NULL) {
    lg::warn("glfwSetErrorCallback has been re-set!");
  }

  if (glfwInit() == GLFW_FALSE) {
    lg::error("glfwInit error");
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  if (!glfwVulkanSupported()) {
    lg::error("Vulkan Not Supportd!");
    return 1;
  }

  return 0;
}

static std::shared_ptr<GfxDisplay> vk_make_display(int width,
                                                   int height,
                                                   const char* title,
                                                   GfxSettings& settings,
                                                   GameVersion game_version,
                                                   bool is_main) {
  GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);

  if (!window) {
    lg::error("gl_make_display failed - Could not create display window");
    return NULL;
  }

  // if (!gl_inited) {
  //  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
  //  if (!gladLoadGL()) {
  //    lg::error("GL init fail");
  //    return NULL;
  //  }
  //  g_gfx_data = std::make_unique<GraphicsData>(game_version);
  //
  //  gl_inited = true;
  //}
  if (g_gfx_data == nullptr) {
  }
  g_gfx_data = std::make_unique<GraphicsData>(game_version);

  // window icon
  std::string image_path =
      (file_util::get_jak_project_dir() / "game" / "assets" / "appicon.png").string();

  GLFWimage images[1];
  auto load_result = stbi_load(image_path.c_str(), &images[0].width, &images[0].height, 0, 4);
  if (load_result) {
    images[0].pixels = load_result;  // rgba channels
    glfwSetWindowIcon(window, 1, images);
    stbi_image_free(images[0].pixels);
  } else {
    lg::error("Could not load icon for OpenGL window");
  }

  GLFWDisplay::SetGlobalGLFWCallbacks();
  Pad::initialize();

  {
    /*  Get Latest Vulkan version. */
    uint32_t version;
    vkEnumerateInstanceVersion(&version);

    /*	Primary Vulkan instance Object. */  // TODO add support to override by user.
    VkApplicationInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pNext = VK_NULL_HANDLE;
    ai.pApplicationName = "OpenGAL";
    ai.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    ai.pEngineName = "OpenGAL Engine";
    ai.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    ai.apiVersion = version;

    uint32_t count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);

    /*  Add Validation layer only for debug mode. */
    std::vector<const char*> useValidationLayers;
    if (settings.debug) {
      useValidationLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    // VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    // if (enableValidationLayers) {

    //     populateDebugMessengerCreateInfo(debugCreateInfo);
    //     createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    // } else {
    //     createInfo.enabledLayerCount = 0;

    //     createInfo.pNext = nullptr;
    // }

    /*	Prepare the instance object. */
    VkInstanceCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pNext = nullptr;
    ici.flags = 0;
    ici.pApplicationInfo = &ai;
    /*	*/
    ici.enabledLayerCount = useValidationLayers.size();
    ici.ppEnabledLayerNames = useValidationLayers.data();
    /*	*/
    ici.enabledExtensionCount = count;
    ici.ppEnabledExtensionNames = extensions;

    /*	Create Vulkan instance.	*/
    vkCreateInstance(&ici, VK_NULL_HANDLE, &g_gfx_data->vulkanCore.instance);
  }
  {
    /*	Get number of physical devices. */
    uint32_t nrPhysicalDevices;
    vkEnumeratePhysicalDevices(g_gfx_data->vulkanCore.instance, &nrPhysicalDevices, VK_NULL_HANDLE);

    g_gfx_data->vulkanCore.physicalDev.resize(nrPhysicalDevices);

    /*  Get all physical devices.    */
    std::vector<VkPhysicalDevice> physical_devices(nrPhysicalDevices);

    vkEnumeratePhysicalDevices(g_gfx_data->vulkanCore.instance, &nrPhysicalDevices,
                               &physical_devices[0]);
    for (size_t i = 0; i < physical_devices.size(); i++) {
      VkPhysicalDevice phDevice = physical_devices[i];
      g_gfx_data->vulkanCore.physicalDev[i].physicalDevices = phDevice;
      /*  Get feature of the device.  */
      vkGetPhysicalDeviceFeatures(phDevice, &g_gfx_data->vulkanCore.physicalDev[i].features);

      /*  Get memory properties.   */
      vkGetPhysicalDeviceMemoryProperties(phDevice,
                                          &g_gfx_data->vulkanCore.physicalDev[i].memProperties);

      /*	Get device properties.	*/
      vkGetPhysicalDeviceProperties(phDevice, &g_gfx_data->vulkanCore.physicalDev[i].properties);

      /*  Select queue family.    */
      uint32_t nrQueueFamilies;
      vkGetPhysicalDeviceQueueFamilyProperties(phDevice, &nrQueueFamilies, VK_NULL_HANDLE);
      g_gfx_data->vulkanCore.physicalDev[i].queueFamilyProperties.resize(nrQueueFamilies);
      vkGetPhysicalDeviceQueueFamilyProperties(
          phDevice, &nrQueueFamilies,
          g_gfx_data->vulkanCore.physicalDev[i].queueFamilyProperties.data());

      uint32_t nrExtensions;
      vkEnumerateDeviceExtensionProperties(phDevice, nullptr, &nrExtensions, nullptr);
      g_gfx_data->vulkanCore.physicalDev[i].extensions.resize(nrExtensions);
      vkEnumerateDeviceExtensionProperties(phDevice, nullptr, &nrExtensions,
                                           g_gfx_data->vulkanCore.physicalDev[i].extensions.data());
    }
  }

  // TODO allow to select device.
  VkPhysicalDevice physicalDevice = g_gfx_data->vulkanCore.physicalDev[0].physicalDevices;
  g_gfx_data->vulkanCore.selectPhysicalDevice = physicalDevice;

  VkDevice vkDevice;

  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface(g_gfx_data->vulkanCore.instance, (GLFWwindow*)window, nullptr,
                              &surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }

  const VKHelper::QueueFamilyIndices indices = VKHelper::findQueueFamilies(physicalDevice, surface);
  std::vector<uint32_t> uniqueQueueFamilies = {(uint32_t)indices.graphicsFamily,
                                               (uint32_t)indices.presentFamily};

  vkDestroySurfaceKHR(g_gfx_data->vulkanCore.instance, surface, nullptr);

  uint32_t graphicsQueueNodeIndex = UINT32_MAX;
  uint32_t computeQueueNodeIndex = UINT32_MAX;
  uint32_t presentQueueNodeIndex = UINT32_MAX;
  uint32_t transferQueueNodeIndex = UINT32_MAX;

  VkQueueFlags requiredQueues = VK_QUEUE_GRAPHICS_BIT;

  uint32_t nrQueues = 1;

  for (size_t i = 0; i < g_gfx_data->vulkanCore.physicalDev[1].queueFamilyProperties.size(); i++) {
    /*  */
    const VkQueueFamilyProperties& familyProp =
        g_gfx_data->vulkanCore.physicalDev[1].queueFamilyProperties[i];
    /*  */
    if ((familyProp.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
        (requiredQueues & VK_QUEUE_GRAPHICS_BIT) != 0) {
      if (graphicsQueueNodeIndex == UINT32_MAX) {
        graphicsQueueNodeIndex = i;
      }
    }
    if ((familyProp.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 &&
        (requiredQueues & VK_QUEUE_COMPUTE_BIT) != 0) {
      if (computeQueueNodeIndex == UINT32_MAX) {
        computeQueueNodeIndex = i;
      }
    }
  }

  std::vector<VkDeviceQueueCreateInfo> queueCreations(nrQueues);
  std::vector<float> queuePriorities(1.0f, nrQueues);
  for (size_t i = 0; i < queueCreations.size(); i++) {
    VkDeviceQueueCreateInfo& queueCreateInfo = queueCreations[i];
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = nullptr;
    queueCreateInfo.flags = 0;
    queueCreateInfo.queueFamilyIndex = graphicsQueueNodeIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriorities[i];
  }

  std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  std::vector<const char*> validationLayers;
  bool enableValidationLayers = true;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreations.size());
  createInfo.pQueueCreateInfos = queueCreations.data();

  createInfo.pEnabledFeatures = nullptr;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  if (enableValidationLayers) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &vkDevice) != VK_SUCCESS) {
    lg::error("failed to create logical device!");
    return NULL;
  }

  // if (HasError()) {
  //  lg::error("gl_make_display error");
  //  return NULL;
  //}

  auto display = std::make_shared<VKDisplay>(window, is_main, g_gfx_data->vulkanCore, vkDevice);
  display->set_imgui_visible(Gfx::g_debug_settings.show_imgui);
  // display->update_cursor_visibility(window, display->is_imgui_visible());
  // lg::debug("init display #x{:x}", (uintptr_t)display);

  // setup imgui
  {
    // check that version of the library is okay
    IMGUI_CHECKVERSION();

    // this does initialization for stuff like the font data
    ImGui::CreateContext();

    // Init ImGui settings
    g_gfx_data->imgui_filename = file_util::get_file_path({"imgui.ini"});
    g_gfx_data->imgui_log_filename = file_util::get_file_path({"imgui_log.txt"});
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = g_gfx_data->imgui_filename.c_str();
    io.LogFilename = g_gfx_data->imgui_log_filename.c_str();

    // set up to get inputs for this window
    // ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplGlfw_InitForVulkan(window, false);

    // NOTE: imgui's setup calls functions that may fail intentionally, and attempts to disable
    // error reporting so these errors are invisible. But it does not work, and some weird X11
    // default cursor error is set here that we clear.
    glfwGetError(NULL);

    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = 11;  // pool_sizes;
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    vkCreateDescriptorPool(display->get_device(), &pool_info, nullptr, &imguiPool);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_gfx_data->vulkanCore.instance;
    init_info.PhysicalDevice = g_gfx_data->vulkanCore.selectPhysicalDevice;
    init_info.Device = display->get_device();
    init_info.Queue = display->getDefaultGraphicQueue();
    ;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // set up the renderer
    ImGui_ImplVulkan_Init(&init_info, display->get_default_render_pass());

    VkCommandBuffer command_buffer = display->beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    display->endSingleTimeCommands(command_buffer);

    // clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }

  return std::static_pointer_cast<GfxDisplay>(display);
}

void ClearGlobalGLFWCallbacks() {
  if (!g_gfx_data->g_glfw_state.callbacks_registered) {
    return;
  }

  glfwSetMonitorCallback(NULL);

  g_gfx_data->g_glfw_state.callbacks_registered = false;
}

void vk_exit() {
  ClearGlobalGLFWCallbacks();
  // g_gfx_data.reset();
  glfwTerminate();
  glfwSetErrorCallback(NULL);
  // gl_inited = false;
}

u32 vk_vsync() {
  if (!g_gfx_data) {
    return 0;
  }
  std::unique_lock<std::mutex> lock(g_gfx_data->sync_mutex);
  auto init_frame = g_gfx_data->frame_idx_of_input_data;
  g_gfx_data->sync_cv.wait(lock, [=] {
    return (MasterExit != RuntimeExitStatus::RUNNING) || g_gfx_data->frame_idx > init_frame;
  });
  return g_gfx_data->frame_idx & 1;
}

u32 vk_sync_path() {
  if (!g_gfx_data) {
    return 0;
  }
  std::unique_lock<std::mutex> lock(g_gfx_data->sync_mutex);
  g_gfx_data->last_engine_time = g_gfx_data->engine_timer.getSeconds();
  if (!g_gfx_data->has_data_to_render) {
    return 0;
  }
  g_gfx_data->sync_cv.wait(lock, [=] { return !g_gfx_data->has_data_to_render; });
  return 0;
}

/*!
 * Send DMA to the renderer.
 * Called from the game thread, on a GOAL stack.
 */
void vk_send_chain(const void* data, u32 offset) {
  if (g_gfx_data) {
    std::unique_lock<std::mutex> lock(g_gfx_data->dma_mutex);
    if (g_gfx_data->has_data_to_render) {
      lg::error(
          "Gfx::send_chain called when the graphics renderer has pending data. Was this called "
          "multiple times per frame?");
      return;
    }

    // we copy the dma data and give a copy of it to the render.
    // the copy has a few advantages:
    // - if the game code has a bug and corrupts the DMA buffer, the renderer won't see it.
    // - the copied DMA is much smaller than the entire game memory, so it can be dumped to a
    // file
    //    separate of the entire RAM.
    // - it verifies the DMA data is valid early on.
    // but it may also be pretty expensive. Both the renderer and the game wait on this to
    // complete.

    // The renderers should just operate on DMA chains, so eliminating this step in the future
    // may be easy.

    g_gfx_data->dma_copier.set_input_data(data, offset, run_dma_copy);

    g_gfx_data->has_data_to_render = true;
    g_gfx_data->dma_cv.notify_all();
  }
}

/*!
 * Upload texture outside of main DMA chain.
 * We trust the game to not remove textures that are currently being used, but if the game is messed
 * up, there is a possible race to updating this texture.
 */
void vk_texture_upload_now(const u8* tpage, int mode, u32 s7_ptr) {
  // block
  if (g_gfx_data) {
    // just pass it to the texture pool.
    // the texture pool will take care of locking.
    // we don't want to lock here for the entire duration of the conversion.
    // g_gfx_data->texture_pool->handle_upload_now(tpage, mode, g_ee_main_mem, s7_ptr);
  }
}

/*!
 * Handle a local->local texture copy. The texture pool can just update texture pointers.
 * This is called from the main thread and the texture pool itself will handle locking.
 */
void vk_texture_relocate(u32 destination, u32 source, u32 format) {
  if (g_gfx_data) {
    // g_gfx_data->texture_pool->relocate(destination, source, format);
  }
}

void vk_poll_events() {
  glfwPollEvents();
}

void vk_set_levels(const std::vector<std::string>& levels) {
  g_gfx_data->loader->set_want_levels(levels);
}

void vk_set_pmode_alp(float val) {
  g_gfx_data->pmode_alp = val;
}

const GfxRendererModule gRendererVulkan = {
    vk_init,                // init
    vk_make_display,        // make_display
    vk_exit,                // exit
    vk_vsync,               // vsync
    vk_sync_path,           // sync_path
    vk_send_chain,          // send_chain
    vk_texture_upload_now,  // texture_upload_now
    vk_texture_relocate,    // texture_relocate
    vk_poll_events,         // poll_events
    vk_set_levels,          // set_levels
    vk_set_pmode_alp,       // set_pmode_alp
    GfxPipeline::Vulkan,    // pipeline
    "Vulkan 1.1"            // name
};