#include "game/graphics/pipelines/vulkan/vulkandisplay.h"

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
#include "game/graphics/pipelines/vulkan/VKHelper.h"
#include "game/graphics/pipelines/vulkan/vulkanmodule.h"
#include "game/graphics/texture/TexturePool.h"
#include "game/graphics/vulkan_renderer/vulkanrenderer.h"
#include "game/runtime.h"
#include "game/sce/libscf.h"
#include "game/system/newpad.h"

#include "third-party/fmt/core.h"
#include "third-party/glfw/include/GLFW/glfw3.h"
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_impl_glfw.h"
#include "third-party/imgui/imgui_impl_vulkan.h"

#define STBI_WINDOWS_UTF8
#include "third-party/stb_image/stb_image.h"

using namespace fvkcore;

constexpr bool run_dma_copy = false;

std::unique_ptr<GraphicsData> g_gfx_data;

VKDisplay::VKDisplay(GLFWwindow* window, bool is_main, VulkanCore& vulkanCore, VkDevice device)
    : GLFWDisplay(window, is_main), vulkanCore(vulkanCore) {
  this->m_main = is_main;
  this->m_device = device;

  {
    /*	*/
    this->swapChain = new SwapchainBuffers();

    if (glfwCreateWindowSurface(this->vulkanCore.instance, (GLFWwindow*)this->get_window(), nullptr,
                                &this->surface) != VK_SUCCESS) {
      throw std::runtime_error("failed to create window surface!");
    }

    const VKHelper::QueueFamilyIndices indices =
        VKHelper::findQueueFamilies(vulkanCore.selectPhysicalDevice, this->surface);
    std::vector<uint32_t> queueFamilyIndices = {(uint32_t)indices.graphicsFamily,
                                                (uint32_t)indices.presentFamily};

    this->graphics_queue_node_index = indices.graphicsFamily;

    vkGetDeviceQueue(this->get_device(), indices.graphicsFamily, 0, &queue);
    vkGetDeviceQueue(this->get_device(), indices.presentFamily, 0, &presentQueue);

    //*  Create command pool.    */
    VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
    cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCreateInfo.queueFamilyIndex = this->getGraphicQueueIndex();
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    /*  Create command pool.    */
    vkCreateCommandPool(this->get_device(), &cmdPoolCreateInfo, NULL, &this->cmd_pool);

    /*	Create swap chain.	*/
    this->createSwapChain();

    const size_t MAX_FRAMES_IN_FLIGHT = this->getSwapChainImageCount();
    this->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    this->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    this->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    /*	*/
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    /*	*/
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vkCreateSemaphore(get_device(), &semaphoreInfo, nullptr, &this->imageAvailableSemaphores[i]);
      vkCreateSemaphore(get_device(), &semaphoreInfo, nullptr, &this->renderFinishedSemaphores[i]);
      vkCreateFence(get_device(), &fenceInfo, nullptr, &this->inFlightFences[i]);
    }
  }

  this->set_imgui_visible(true);
}

VKDisplay::~VKDisplay() {
  // TODO cleanup vulkan resources.
  /*  */
  {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }
  glfwDestroyWindow(this->m_window);
  if (m_main) {
    // vk_exit();
  }
}

void VKDisplay::on_window_size(GLFWwindow* window, int width, int height) {
  GLFWDisplay::on_window_size(window, width, height);
  recreateSwapChain();
}

void render_game_frame(int game_width,
                       int game_height,
                       int window_fb_width,
                       int window_fb_height,
                       int draw_region_width,
                       int draw_region_height,
                       int msaa_samples,
                       bool windows_borderless_hack,
                       VkCommandBuffer secondCommandbuffer) {
  // wait for a copied chain.
  bool got_chain = false;
  {
    auto p = scoped_prof("wait-for-dma");
    std::unique_lock<std::mutex> lock(g_gfx_data->dma_mutex);
    // note: there's a timeout here. If the engine is messed up and not sending us frames,
    // we still want to run the glfw loop.
    got_chain = g_gfx_data->dma_cv.wait_for(lock, std::chrono::milliseconds(50),
                                            [=] { return g_gfx_data->has_data_to_render; });
  }
  // render that chain.
  if (got_chain) {
    g_gfx_data->frame_idx_of_input_data = g_gfx_data->frame_idx;
    RenderOptions options;
    options.game_res_w = game_width;
    options.game_res_h = game_height;
    options.window_framebuffer_width = window_fb_width;
    options.window_framebuffer_height = window_fb_height;
    options.draw_region_width = draw_region_width;
    options.draw_region_height = draw_region_height;
    options.msaa_samples = msaa_samples;
    options.draw_render_debug_window = g_gfx_data->debug_gui.should_draw_render_debug();
    options.draw_profiler_window = g_gfx_data->debug_gui.should_draw_profiler();
    options.draw_loader_window = g_gfx_data->debug_gui.should_draw_loader_menu();
    options.draw_subtitle_editor_window = g_gfx_data->debug_gui.should_draw_subtitle_editor();
    options.draw_filters_window = g_gfx_data->debug_gui.should_draw_filters_menu();
    options.save_screenshot = false;
    options.gpu_sync = g_gfx_data->debug_gui.should_gl_finish();
    options.borderless_windows_hacks = windows_borderless_hack;

    // if (want_hotkey_screenshot && g_gfx_data->debug_gui.screenshot_hotkey_enabled) {
    //  options.save_screenshot = true;
    //  std::string screenshot_file_name = make_hotkey_screenshot_file_name();
    //  options.screenshot_path = make_full_screenshot_output_file_path(screenshot_file_name);
    //}
    // if (g_gfx_data->debug_gui.get_screenshot_flag()) {
    //  options.save_screenshot = true;
    //  options.game_res_w = g_gfx_data->debug_gui.screenshot_width;
    //  options.game_res_h = g_gfx_data->debug_gui.screenshot_height;
    //  options.draw_region_width = options.game_res_w;
    //  options.draw_region_height = options.game_res_h;
    //  options.msaa_samples = g_gfx_data->debug_gui.screenshot_samples;
    //  std::string screenshot_file_name = g_gfx_data->debug_gui.screenshot_name();
    //  if (!endsWith(screenshot_file_name, ".png")) {
    //    screenshot_file_name += ".png";
    //  }
    //  options.screenshot_path = make_full_screenshot_output_file_path(screenshot_file_name);
    //}
    // want_hotkey_screenshot = false;

    options.draw_small_profiler_window =
        g_gfx_data->debug_gui.master_enable && g_gfx_data->debug_gui.small_profiler;
    options.pmode_alp_register = g_gfx_data->pmode_alp;

    GLint msaa_max;
    glGetIntegerv(GL_MAX_SAMPLES, &msaa_max);
    if (options.msaa_samples > msaa_max) {
      options.msaa_samples = msaa_max;
    }

    if constexpr (run_dma_copy) {
      auto& chain = g_gfx_data->dma_copier.get_last_result();
      g_gfx_data->ogl_renderer.render(DmaFollower(chain.data.data(), chain.start_offset),
                                      secondCommandbuffer);
    } else {
      auto p = scoped_prof("ogl-render");

      g_gfx_data->ogl_renderer.render(DmaFollower(g_gfx_data->dma_copier.get_last_input_data(),
                                                  g_gfx_data->dma_copier.get_last_input_offset()),
                                      secondCommandbuffer);  //,
                                                             //  options);
    }
  }

  // before vsync, mark the chain as rendered.
  {
    // should be fine to remove this mutex if the game actually waits for vsync to call
    // send_chain again. but let's be safe for now.
    std::unique_lock<std::mutex> lock(g_gfx_data->dma_mutex);
    g_gfx_data->engine_timer.start();
    g_gfx_data->has_data_to_render = false;
    g_gfx_data->sync_cv.notify_all();
  }
}

void VKDisplay::render() {
  update_glfw();

  // imgui start of frame
  {
    auto p = scoped_prof("imgui-init");
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
  }

  // render game!
  g_gfx_data->debug_gui.master_enable = is_imgui_visible();
  if (g_gfx_data->debug_gui.should_advance_frame()) {
    auto p = scoped_prof("game-render");
    int game_res_w = Gfx::g_global_settings.game_res_w;
    int game_res_h = Gfx::g_global_settings.game_res_h;
    if (game_res_w <= 0 || game_res_h <= 0) {
      // if the window size reports 0, the game will ask for a 0 sized window, and nothing likes
      // that.
      game_res_w = 640;
      game_res_h = 480;
    }

    render_game_frame(game_res_w, game_res_h, width(), height(), Gfx::g_global_settings.lbox_w,
                      Gfx::g_global_settings.lbox_h, Gfx::g_global_settings.msaa_samples, false,
                      this->graphicCommandBuffers[this->swapChain->currentFrame]);
  }

  // render debug
  if (is_imgui_visible()) {
    auto p = scoped_prof("debug-gui");
    g_gfx_data->debug_gui.draw(g_gfx_data->dma_copier.get_last_result().stats);
  }

  {
    auto p = scoped_prof("imgui-render");
    ImGui::Render();

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;

    vkResetCommandBuffer(this->swapChain->commandBuffers[this->swapChain->currentFrame], 0);

    vkBeginCommandBuffer(this->swapChain->commandBuffers[this->swapChain->currentFrame],
                         &beginInfo);

    // TODO move to the renderer.
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = this->swapChain->renderPass;
    renderPassInfo.framebuffer =
        this->swapChain->swapChainFramebuffers[this->swapChain->currentFrame];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent.width = width();
    renderPassInfo.renderArea.extent.height = height();

    /*	*/
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.1f, 0.1f, 0.1f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = clearValues.size();
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(this->swapChain->commandBuffers[this->swapChain->currentFrame],
                         &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdExecuteCommands(this->swapChain->commandBuffers[this->swapChain->currentFrame], 1,
                         &this->graphicCommandBuffers[this->swapChain->currentFrame]);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                    this->swapChain->commandBuffers[this->swapChain->currentFrame]);

    vkCmdEndRenderPass(this->swapChain->commandBuffers[this->swapChain->currentFrame]);

    vkEndCommandBuffer(this->swapChain->commandBuffers[this->swapChain->currentFrame]);
  }

  // update fullscreen mode, if requested
  //{
  //  auto p = scoped_prof("fullscreen-update");
  //  update_last_fullscreen_mode();
  //
  //  if (this->fullscreen_pending() && !minimized()) {
  //    this->fullscreen_flush();
  //  }
  //}

  // actual vsync
  g_gfx_data->debug_gui.finish_frame();
  if (Gfx::g_global_settings.framelimiter) {
    auto p = scoped_prof("frame-limiter");
    g_gfx_data->frame_limiter.run(
        Gfx::g_global_settings.target_fps, Gfx::g_global_settings.experimental_accurate_lag,
        Gfx::g_global_settings.sleep_in_frame_limiter, g_gfx_data->last_engine_time);
  }
  {
    auto p = scoped_prof("swap-buffers");
    swap_buffer();
  }
}

void VKDisplay::swap_buffer() {
  VkResult result;

  vkWaitForFences(get_device(), 1, &this->inFlightFences[this->swapChain->currentFrame], VK_TRUE,
                  UINT64_MAX);

  /*  */
  uint32_t imageIndex;
  result = vkAcquireNextImageKHR(get_device(), this->swapChain->swapchain, UINT64_MAX,
                                 this->imageAvailableSemaphores[this->swapChain->currentFrame],
                                 VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain();
    return;
  } else
    VKS_VALIDATE(result);

  if (this->imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(get_device(), 1, &this->imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }
  /*	*/
  this->imagesInFlight[imageIndex] = this->inFlightFences[this->swapChain->currentFrame];

  /*	*/
  VkSemaphore signalSemaphores[] = {this->renderFinishedSemaphores[this->swapChain->currentFrame]};

  vkResetFences(get_device(), 1, &this->inFlightFences[this->swapChain->currentFrame]);

  submitCommands(getDefaultGraphicQueue(), {this->swapChain->commandBuffers[imageIndex]},
                 {this->imageAvailableSemaphores[this->swapChain->currentFrame]},
                 {this->renderFinishedSemaphores[this->swapChain->currentFrame]},
                 this->inFlightFences[this->swapChain->currentFrame],
                 {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT});

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  /*	*/
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &this->swapChain->swapchain;

  /*	*/
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  /*	*/
  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(this->getDefaultGraphicQueue(), &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // framebufferResized = false;
    recreateSwapChain();
  } else if (result != VK_SUCCESS) {
    //  throw cxxexcept::RuntimeException("failed to present swap chain image!");
  }

  /*  Compute current frame.  */
  this->swapChain->currentFrame =
      (this->swapChain->currentFrame + 1) %
      std::min((uint32_t)this->inFlightFences.size(), (uint32_t)this->getSwapChainImageCount());
}

void VKDisplay::createSwapChain() {
  /*	*/
  VkPhysicalDevice physicalDevice =
      this->vulkanCore.selectPhysicalDevice;  // device->getPhysicalDevice(0);

  /*  */
  VKHelper::SwapChainSupportDetails swapChainSupport =
      VKHelper::querySwapChainSupport(physicalDevice, this->surface);

  const std::vector<VkSurfaceFormatKHR> requestFormat = {
      ///{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
      {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};

  /*	TODO add option support.	*/
  VkSurfaceFormatKHR surfaceFormat = VKHelper::selectSurfaceFormat(
      swapChainSupport.formats, requestFormat, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

  // TODO add support to determine which present mode.
  std::vector<VkPresentModeKHR> requestedPresentModes = {
      VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR};
  const VkPresentModeKHR presentMode =
      VKHelper::chooseSwapPresentMode(swapChainSupport.presentModes, requestedPresentModes);
  const VkExtent2D extent = VKHelper::chooseSwapExtent(
      swapChainSupport.capabilities, {(uint32_t)this->width(), (uint32_t)this->height()});

  /*	Reset frame counter.	*/
  this->swapChain->currentFrame = 0;

  /*	TODO evoluate if this is thec correct.	*/
  /*	Compute number of image to use in the swapchain.	*/
  uint32_t imageCount = std::max((uint32_t)swapChainSupport.capabilities.minImageCount,
                                 (uint32_t)1); /*	Atleast one.	*/
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
    /*	Clamp it to number of semaphores/fences.	*/
    imageCount =
        std::max(swapChainSupport.capabilities.maxImageCount, (uint32_t)imagesInFlight.size());
  }

  /*	*/
  const VKHelper::QueueFamilyIndices indices =
      VKHelper::findQueueFamilies(physicalDevice, this->surface);
  std::vector<uint32_t> queueFamilyIndices = {(uint32_t)indices.graphicsFamily,
                                              (uint32_t)indices.presentFamily};

  /*  */
  VkSwapchainCreateInfoKHR createSwapChainInfo = {};
  createSwapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createSwapChainInfo.pNext = nullptr;
  createSwapChainInfo.surface = this->surface;

  /*  Image configurations.	*/
  createSwapChainInfo.minImageCount = imageCount;
  createSwapChainInfo.imageFormat = surfaceFormat.format;
  createSwapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
  createSwapChainInfo.imageExtent = extent;
  createSwapChainInfo.imageArrayLayers = 1;
  createSwapChainInfo.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  /*  */
  if (indices.graphicsFamily != indices.presentFamily) {
    createSwapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createSwapChainInfo.queueFamilyIndexCount = queueFamilyIndices.size();
    createSwapChainInfo.pQueueFamilyIndices = queueFamilyIndices.data();
  } else {
    createSwapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  /*  */
  createSwapChainInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  createSwapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createSwapChainInfo.presentMode = presentMode;
  createSwapChainInfo.clipped = VK_TRUE;

  /*	*/
  createSwapChainInfo.oldSwapchain = VK_NULL_HANDLE;

  /*  Create swapchain.   */
  VKS_VALIDATE(vkCreateSwapchainKHR(get_device(), &createSwapChainInfo, nullptr,
                                    &this->swapChain->swapchain));

  /*  Get the image associated with the swap chain.   */
  uint32_t nrChainImageCount = 1;
  VKS_VALIDATE(vkGetSwapchainImagesKHR(get_device(), this->swapChain->swapchain, &nrChainImageCount,
                                       nullptr));

  this->swapChain->swapChainImages.resize(nrChainImageCount);
  VKS_VALIDATE(vkGetSwapchainImagesKHR(get_device(), this->swapChain->swapchain, &nrChainImageCount,
                                       this->swapChain->swapChainImages.data()));

  this->swapChain->swapChainImageFormat = surfaceFormat.format;
  this->swapChain->chainExtend = extent;
  this->imagesInFlight.resize(this->swapChain->swapChainImages.size(), VK_NULL_HANDLE);

  /*	*/
  this->swapChain->swapChainImageViews.resize(this->swapChain->swapChainImages.size());

  for (size_t i = 0; i < this->swapChain->swapChainImages.size(); i++) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = this->swapChain->swapChainImages[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = this->swapChain->swapChainImageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VKS_VALIDATE(vkCreateImageView(get_device(), &createInfo, nullptr,
                                   &this->swapChain->swapChainImageViews[i]));
  }

  VkFormat depthFormat = findDepthFormat();

  const VkPhysicalDeviceMemoryProperties& memProps = vulkanCore.physicalDev[0].memProperties;

  VKHelper::createImage(get_device(), this->swapChain->chainExtend.width,
                        this->swapChain->chainExtend.height, 1, depthFormat,
                        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memProps, this->swapChain->depthImage,
                        this->swapChain->depthImageMemory);

  this->swapChain->depthImageView =
      VKHelper::createImageView(get_device(), this->swapChain->depthImage, VK_IMAGE_VIEW_TYPE_2D,
                                depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

  /*	Renderpass	*/
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = this->swapChain->swapChainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = depthFormat;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = attachments.size();
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  VKS_VALIDATE(
      vkCreateRenderPass(get_device(), &renderPassInfo, nullptr, &this->swapChain->renderPass));

  /*	Framebuffer.	*/
  // TODO add support.
  this->swapChain->swapChainFramebuffers.resize(this->swapChain->swapChainImageViews.size());

  for (size_t i = 0; i < this->swapChain->swapChainImageViews.size(); i++) {
    std::array<VkImageView, 2> attachments = {this->swapChain->swapChainImageViews[i],
                                              this->swapChain->depthImageView};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = this->swapChain->renderPass;
    framebufferInfo.attachmentCount = attachments.size();
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = this->swapChain->chainExtend.width;
    framebufferInfo.height = this->swapChain->chainExtend.height;
    framebufferInfo.layers = 1;

    VKS_VALIDATE(vkCreateFramebuffer(this->get_device(), &framebufferInfo, nullptr,
                                     &this->swapChain->swapChainFramebuffers[i]));
  }

  /*	Command buffers*/
  this->swapChain->commandBuffers.resize(this->swapChain->swapChainFramebuffers.size());
  VkCommandBufferAllocateInfo cmdBufAllocInfo = {};
  cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmdBufAllocInfo.commandPool = this->cmd_pool;
  cmdBufAllocInfo.commandBufferCount = this->swapChain->swapChainImages.size();
  cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  VKS_VALIDATE(vkAllocateCommandBuffers(this->get_device(), &cmdBufAllocInfo,
                                        this->swapChain->commandBuffers.data()));

  this->graphicCommandBuffers.resize(this->swapChain->swapChainFramebuffers.size());
  VKS_VALIDATE(vkAllocateCommandBuffers(this->get_device(), &cmdBufAllocInfo,
                                        this->graphicCommandBuffers.data()));
}

void VKDisplay::recreateSwapChain() {
  vkDeviceWaitIdle(this->get_device());

  cleanSwapChain();

  createSwapChain();
}

void VKDisplay::cleanSwapChain() {
  for (auto framebuffer : swapChain->swapChainFramebuffers) {
    vkDestroyFramebuffer(get_device(), framebuffer, nullptr);
  }
  swapChain->swapChainFramebuffers.clear();

  /*	*/
  vkFreeCommandBuffers(get_device(), this->cmd_pool,
                       static_cast<uint32_t>(swapChain->commandBuffers.size()),
                       swapChain->commandBuffers.data());
  swapChain->commandBuffers.clear();

  vkDestroyRenderPass(get_device(), swapChain->renderPass, nullptr);

  /*	*/
  for (auto imageView : swapChain->swapChainImageViews) {
    vkDestroyImageView(get_device(), imageView, nullptr);
  }
  swapChain->swapChainImageViews.clear();

  /*	Release depth/stencil.	*/
  vkDestroyImageView(get_device(), swapChain->depthImageView, nullptr);
  vkDestroyImage(get_device(), swapChain->depthImage, nullptr);
  vkFreeMemory(get_device(), swapChain->depthImageMemory, nullptr);

  vkDestroySwapchainKHR(get_device(), this->swapChain->swapchain, nullptr);
}

VkFormat VKDisplay::findDepthFormat() {
  return VKHelper::findSupportedFormat(
      physicalDevice(),
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkDevice VKDisplay::get_device() const {
  return this->m_device;
}
