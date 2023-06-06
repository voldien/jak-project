#include "ShaderLibrary.h"
#include "common/log/log.h"
#include "common/util/Assert.h"
#include "common/util/FileUtil.h"

ShaderLibrary::ShaderLibrary(GameVersion version) {
  at(ShaderId::SOLID_COLOR) = {"solid_color", version};
  at(ShaderId::DIRECT_BASIC) = {"direct_basic", version};
  at(ShaderId::DIRECT_BASIC_TEXTURED) = {"direct_basic_textured", version};
  at(ShaderId::DIRECT_BASIC_TEXTURED_MULTI_UNIT) = {"direct_basic_textured_multi_unit", version};
  at(ShaderId::DEBUG_RED) = {"debug_red", version};
  at(ShaderId::SPRITE) = {"sprite_3d", version};
  at(ShaderId::SKY) = {"sky", version};
  at(ShaderId::SKY_BLEND) = {"sky_blend", version};
  at(ShaderId::TFRAG3) = {"tfrag3", version};
  at(ShaderId::TFRAG3_NO_TEX) = {"tfrag3_no_tex", version};
  at(ShaderId::SPRITE3) = {"sprite3_3d", version};
  at(ShaderId::DIRECT2) = {"direct2", version};
  at(ShaderId::EYE) = {"eye", version};
  at(ShaderId::GENERIC) = {"generic", version};
  at(ShaderId::OCEAN_TEXTURE) = {"ocean_texture", version};
  at(ShaderId::OCEAN_TEXTURE_MIPMAP) = {"ocean_texture_mipmap", version};
  at(ShaderId::OCEAN_COMMON) = {"ocean_common", version};
  at(ShaderId::SHRUB) = {"shrub", version};
  at(ShaderId::SHADOW) = {"shadow", version};
  at(ShaderId::COLLISION) = {"collision", version};
  at(ShaderId::MERC2) = {"merc2", version};
  at(ShaderId::SPRITE_DISTORT) = {"sprite_distort", version};
  at(ShaderId::SPRITE_DISTORT_INSTANCED) = {"sprite_distort_instanced", version};
  at(ShaderId::POST_PROCESSING) = {"post_processing", version};
  at(ShaderId::DEPTH_CUE) = {"depth_cue", version};
  at(ShaderId::EMERC) = {"emerc", version};
  at(ShaderId::GLOW_PROBE) = {"glow_probe", version};
  at(ShaderId::GLOW_PROBE_READ) = {"glow_probe_read", version};
  at(ShaderId::GLOW_PROBE_READ_DEBUG) = {"glow_probe_read_debug", version};
  at(ShaderId::GLOW_PROBE_DOWNSAMPLE) = {"glow_probe_downsample", version};
  at(ShaderId::GLOW_DRAW) = {"glow_draw", version};
  at(ShaderId::ETIE_BASE) = {"etie_base", version};
  at(ShaderId::ETIE) = {"etie", version};
  at(ShaderId::SHADOW2) = {"shadow2", version};

  for (auto& shader : m_shaders) {
    ASSERT_MSG(shader.okay(), "error compiling shader");
  }
}
