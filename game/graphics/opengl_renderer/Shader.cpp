#include "Shader.h"

#include "common/log/log.h"
#include "common/util/Assert.h"
#include "common/util/FileUtil.h"

#include "game/graphics/opengl_renderer/GlobalConstant.h"
#include "game/graphics/pipelines/opengl/opengl.h"
#include "game/graphics/shadercompiler.h"

Shader::Shader(const std::string& shader_name, GameVersion version) : m_name(shader_name) {
  const std::string height_scale = version == GameVersion::Jak1 ? "1.0" : "0.5";
  const std::string scissor_height = version == GameVersion::Jak1 ? "448.0" : "416.0";
  const std::string scissor_adjust = "512.0 / " + scissor_height;

  // read the shader source
  auto binary_vert_src = file_util::read_binary_file(
      file_util::get_file_path({shader_folder, shader_name + ".vert.spv"}));
  auto binary_frag_src = file_util::read_binary_file(
      file_util::get_file_path({shader_folder, shader_name + ".frag.spv"}));

  ShaderCompiler::CompilerConvertOption compilerOptions;
  compilerOptions.target = ShaderCompiler::ShaderLanguage::GLSL;
  compilerOptions.glslVersion = 330;

  std::vector<char> vert_src = ShaderCompiler::convertSPIRV(binary_vert_src, compilerOptions);
  std::vector<char> frag_src = ShaderCompiler::convertSPIRV(binary_frag_src, compilerOptions);

  m_vert_shader = glCreateShader(GL_VERTEX_SHADER);
  const char* src = &vert_src[0];

  glShaderSource(m_vert_shader, 1, &src, nullptr);
  glCompileShader(m_vert_shader);

  constexpr int len = 1024;
  int compile_ok;
  char err[len];

  glGetShaderiv(m_vert_shader, GL_COMPILE_STATUS, &compile_ok);
  if (!compile_ok) {
    glGetShaderInfoLog(m_vert_shader, len, nullptr, err);
    lg::error("Failed to compile vertex shader {}:\n{}\n\n{}", shader_name.c_str(), err, src);
    m_is_okay = false;
    return;
  }

  m_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
  src = &frag_src[0];
  glShaderSource(m_frag_shader, 1, &src, nullptr);
  glCompileShader(m_frag_shader);

  glGetShaderiv(m_frag_shader, GL_COMPILE_STATUS, &compile_ok);
  if (!compile_ok) {
    glGetShaderInfoLog(m_frag_shader, len, nullptr, err);
    lg::error("Failed to compile fragment shader {}:\n{}\n\n{}", shader_name.c_str(), err, src);
    m_is_okay = false;
    return;
  }

  m_program = glCreateProgram();
  glAttachShader(m_program, m_vert_shader);
  glAttachShader(m_program, m_frag_shader);
  glLinkProgram(m_program);

  glGetProgramiv(m_program, GL_LINK_STATUS, &compile_ok);
  if (!compile_ok) {
    glGetProgramInfoLog(m_program, len, nullptr, err);
    lg::error("Failed to link shader {}:\n{}", shader_name.c_str(), err);
    m_is_okay = false;
    return;
  }

  glDeleteShader(m_vert_shader);
  glDeleteShader(m_frag_shader);
  m_is_okay = true;

  {
    glUseProgram(m_program);
    int uniform_global_buffer_index = glGetUniformBlockIndex(0, "ub_global");
    glUniformBlockBinding(this->m_program, uniform_global_buffer_index,
                          GlobalConstant::getBindingIndex());
    glUseProgram(0);
  }
}

void Shader::activate() const {
  ASSERT(m_is_okay);
  glUseProgram(m_program);
}
