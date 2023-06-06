#version 430 core

layout(location = 0) out vec4 color;

// output
layout(location = 0) in vec3 vtx_color;
layout(location = 1) in vec2 vtx_st;
layout(location = 2) in float fog;

layout(location = 0) uniform sampler2D tex_T0;

uniform vec4 fog_color;
uniform int ignore_alpha;

uniform int decal_enable;

uniform int gfx_hack_no_tex;


void main() {
  if (gfx_hack_no_tex == 0) {
    vec4 T0 = texture(tex_T0, vtx_st);

    color.a = T0.a;
    color.rgb = T0.rgb * vtx_color;
    color *= 2;
  } else {
    color.rgb = vtx_color;
    color.a = 1;
  }
}
