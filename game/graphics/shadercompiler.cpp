#include "game/graphics/shadercompiler.h"

#include <spirv_glsl.hpp>

std::vector<char> ShaderCompiler::convertSPIRV(const std::vector<uint8_t>& source,
                                               const CompilerConvertOption& target) {
  // Read SPIR-V from disk or similar.

  spirv_cross::CompilerGLSL glsl((uint32_t*)source.data(), source.size() / 4);

  // Set some options.
  spirv_cross::CompilerGLSL::Options options;  // = glsl.get_common_options();
  options.version = target.glslVersion;
  options.es = false;
  options.enable_420pack_extension = true;
  //  options.emit_uniform_buffer_as_plain_uniforms = true;
  glsl.set_common_options(options);

  // Compile to GLSL, ready to give to GL driver.
  const std::string converted_source = glsl.compile();

  /*	Includes the null terminator.	*/
  return std::vector<char>(converted_source.begin(), converted_source.end() + 1);
}
