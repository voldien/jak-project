#pragma once

#include <string>

#include "common/common_types.h"
#include "common/versions/versions.h"

#include "graphics/renderer/ShaderLibrary.h"

class Shader {
 public:
  static constexpr char shader_folder[] = "game/graphics/opengl_renderer/shaders/";

  virtual void activate() const;
  virtual bool okay() const { return m_is_okay; }
  virtual u64 id() const = 0;

 private:
  std::string m_name;
  bool m_is_okay = false;
};
