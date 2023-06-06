#pragma once

#include <array>
#include <memory>

#include "common/dma/dma_chain_read.h"

#include "game/graphics/opengl_renderer/BucketRenderer.h"
#include "game/graphics/opengl_renderer/CollideMeshRenderer.h"
#include "game/graphics/opengl_renderer/Profiler.h"
#include "game/graphics/opengl_renderer/Shader.h"
#include "game/graphics/opengl_renderer/opengl_utils.h"
#include "game/tools/filter_menu/filter_menu.h"
#include "game/tools/subtitles/subtitle_editor.h"
#include "vulkan/vulkan.h"

class VulkanRenderer {
 public:
  VulkanRenderer(std::shared_ptr<TexturePool> texture_pool,
                 std::shared_ptr<Loader> loader,
                 GameVersion version);

  // rendering interface: takes the dma chain from the game, and some size/debug settings from
  // the graphics system.
  void render(DmaFollower dma, VkCommandBuffer commandBuffer);  //, const RenderOptions& settings);

 private:
  GameVersion m_version;
  // SharedRenderState m_render_state;
  // CollideMeshRenderer m_collide_renderer;
};