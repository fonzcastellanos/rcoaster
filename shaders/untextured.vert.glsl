#version 150

in vec3 vert_position;
in vec4 vert_color;
out vec4 frag_color;

uniform mat4 model_view;
uniform mat4 projection;

void main() {
  gl_Position = projection * model_view * vec4(vert_position, 1.0f);
  frag_color = vert_color;
}