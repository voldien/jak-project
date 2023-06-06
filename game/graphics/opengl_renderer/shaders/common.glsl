#define HEIGHT_SCALE  globalState.height_scale      // 1.0
#define SCISSOR_HEIGHT   globalState.scissor_height  // 448 //
#define SCISSOR_ADJUST (512.0 / SCISSOR_HEIGHT)

layout(binding = 12, std140) uniform ub_global {
  float camera_far;
  float camera_near;
  float height_scale;
  float scissor_height;
  float window_width;
  float window_height;
  float time;
  float deltatime;
  mat4 viewMatrix;
  vec4 cameraPosition;

  float fog_constant;
  float fog_min;
  float fog_max;
}
globalState;


uniform mat4 camera;
uniform vec4 camera_position;
uniform vec4 fog_color;
uniform float fog_constant;
uniform vec4 fog_constants;
uniform float fog_min;
uniform float fog_max;
uniform int ignore_alpha;
uniform int decal_enable;
uniform int gfx_hack_no_tex;
uniform float fog_hack_threshold;
uniform vec4 hvdf_offset;
uniform mat4 perspective_matrix;