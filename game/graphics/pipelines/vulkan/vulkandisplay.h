
#pragma once

#pragma once

/*!
 * @file vulkan.h
 * Vulkan includes.
 */
#include "game/graphics/pipelines/vulkan/vulkanmodule.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

#include "game/graphics/pipelines/glfw_display.h"



class VKDisplay : public GLFWDisplay {
 public:
  VKDisplay(GLFWwindow* window, bool is_main, VulkanCore& vulkanCore, VkDevice device);
  virtual ~VKDisplay();

  virtual void on_window_size(GLFWwindow* window, int width, int height) override;

  void render() override;

 public: /*  Vulkan Internal Specific Methods. */
  /*	*/
  VkDevice get_device() const;
  VkPhysicalDevice physicalDevice() const {
    return this->vulkanCore.physicalDev[0].physicalDevices;
  }
  uint32_t getSwapChainImageCount() const noexcept {
    return this->swapChain->swapChainImages.size();
  }
  /*	*/
  uint32_t getGraphicQueueIndex() const { return graphics_queue_node_index; }
  VkQueue getDefaultGraphicQueue() const { return this->queue; }

  VkRenderPass get_default_render_pass() const { return this->swapChain->renderPass; }

  VkQueue getDefaultComputeQueue() const { return this->queue; }

  VkCommandBuffer beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = this->cmd_pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(this->get_device(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
  }

  void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(getDefaultGraphicQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(getDefaultGraphicQueue());

    vkFreeCommandBuffers(get_device(), this->cmd_pool, 1, &commandBuffer);
  }

  void submitCommands(VkQueue queue,
                      const std::vector<VkCommandBuffer>& cmd,
                      const std::vector<VkSemaphore>& waitSemaphores = {},
                      const std::vector<VkSemaphore>& signalSempores = {},
                      VkFence fence = VK_NULL_HANDLE,
                      const std::vector<VkPipelineStageFlags>& waitStages = {
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}) {
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();

    /*	*/
    submitInfo.commandBufferCount = cmd.size();
    submitInfo.pCommandBuffers = cmd.data();

    /*	*/
    submitInfo.signalSemaphoreCount = signalSempores.size();
    submitInfo.pSignalSemaphores = signalSempores.data();

    vkQueueSubmit(queue, 1, &submitInfo, fence);
  }

 private:
  void swap_buffer();
  void createSwapChain();
  void recreateSwapChain();
  void cleanSwapChain();
  VkFormat findDepthFormat();

  typedef struct _SwapchainBuffers {
    struct SwapChainSupportDetails {
      VkSurfaceCapabilitiesKHR capabilities;
      std::vector<VkSurfaceFormatKHR> formats;
      std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails details; /*  */

    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    std::vector<VkCommandBuffer> commandBuffers;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    /*	*/
    VkFormat swapChainImageFormat;
    VkRenderPass renderPass;
    VkSwapchainKHR swapchain; /*  */
    VkExtent2D chainExtend;   /*  */
    int currentFrame = 0;
    bool vsync = false;
  } SwapchainBuffers;

  VkDevice m_device;
  VulkanCore& vulkanCore;
  VkSurfaceKHR surface;

  /*  Collection of swap chain variables. */
  SwapchainBuffers* swapChain;  // TODO remove as pointer

  std::vector<VkCommandBuffer> graphicCommandBuffers;

  /*  */
  VkQueue queue;  // TODO rename graphicsQueue
  VkQueue presentQueue;

  /*  */
  uint32_t graphics_queue_node_index = 1;
  VkCommandPool cmd_pool;
  VkCommandPool compute_pool;
  VkCommandPool transfer_pool;

  /*  Synchronization.	*/
  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  std::vector<VkFence> imagesInFlight;
  std::vector<VkFence> imageAvailableFence;
};
