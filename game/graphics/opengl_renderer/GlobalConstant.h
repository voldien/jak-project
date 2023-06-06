#pragma once

#include "game/graphics/opengl_renderer/opengl_utils.h"

class GlobalConstant {
 public:
  GlobalConstant();
  ~GlobalConstant();

  void uploadConstant();
  void bind();
  static unsigned int getBindingIndex();

  typedef struct shader_global_t {
    float camera_far;
    float camera_near;
    float height_scale = 1.0f;
    float scissor_height = 448.0f;
    float window_width;
    float window_height;
    float time;
    float deltatime;
    float viewMatrix[16];
    float persMatrix[16];
    float cameraPosition[4];
    float camera_hvdf_off[4];

    float fog_constant;
    float fog_min;
    float fog_max;
  } ShaderGlobal;

  ShaderGlobal Constants;

 private:
  u64 bufferID;
  size_t uniformBufferSize;
  size_t globalSize;
  size_t nrBuffers = 6;
  size_t currentBuffer;
};