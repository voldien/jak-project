#pragma once
#include <map>
#include <string>
#include <vector>

class ShaderCompiler {
 public:
  enum ShaderLanguage {
    GLSL = 0x1,            /*  OpenGL GLSL.    */
    SPIRV = 0x2,           /*  SPIRV.  */
    HLSL = 0x4,            /*  High Level Shading Language.    */
    unKnownLanguage = 0x0, /*	*/
  };

  typedef struct compiler_convert_option_t {
    ShaderLanguage target;
    unsigned int glslVersion;
  } CompilerConvertOption;

  static std::vector<char> convertSPIRV(const std::vector<uint8_t>& source,
                                        const CompilerConvertOption& target);
};
