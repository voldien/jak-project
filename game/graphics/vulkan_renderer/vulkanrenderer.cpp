#include "vulkanrenderer.h"

VulkanRenderer::VulkanRenderer(std::shared_ptr<TexturePool> texture_pool,
                               std::shared_ptr<Loader> loader,
                               GameVersion version)
    :  // m_render_state(texture_pool, loader, version),
       // m_collide_renderer(version),
      m_version(version) {
  // lg::debug("OpenGL context information: {}", (const char*)glGetString(GL_VERSION));

  // initialize all renderers
  switch (m_version) {
    case GameVersion::Jak1:
      // init_bucket_renderers_jak1();
      break;
    case GameVersion::Jak2:
      // init_bucket_renderers_jak2();
      break;
    default:
      ASSERT(false);
  }
}

//, const RenderOptions& settings);

void VulkanRenderer::render(DmaFollower dma, VkCommandBuffer commandBuffer) {
  vkResetCommandBuffer(commandBuffer, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  //  vkCmdWriteTimestamp()

  vkEndCommandBuffer(commandBuffer);
}