#version 430 core

layout(location = 0) out vec4 color;

layout(location = 0) in vec4 fragment_color;

layout(binding = 0) uniform sampler2D tex_T0;

uniform float alpha_min;
uniform float alpha_max;

void main() {
  color = fragment_color;
}
