#define HEIGHT_SCALE globalState.height_scale      // 1.0
#define SCISSOR_HEIGHT globalState.scissor_height  // 448 //
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
  mat4 perspMatrix;
  vec4 cameraPosition;
  vec4 camera_hvdf_off;

  float fog_constant;
  float fog_min;
  float fog_max;
}
globalState;

// As macro in till complete migration to global state buffer.
#define camera globalState.viewMatrix
#define camera_position globalState.cameraPosition
#define hvdf_offset globalState.camera_hvdf_off

uniform vec4 fog_color;
uniform float fog_constant;
uniform vec4 fog_constants;
uniform float fog_min;
uniform float fog_max;
uniform int ignore_alpha;
uniform int decal_enable;
uniform int gfx_hack_no_tex;
uniform float fog_hack_threshold;
uniform mat4 perspective_matrix;