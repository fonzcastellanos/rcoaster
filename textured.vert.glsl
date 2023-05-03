#version 150

in vec3 vert_position;
in vec2 vert_tex_coord;

out vec2 frag_tex_coord;

uniform mat4 model_view;
uniform mat4 projection;

void main()
{
  gl_Position = projection * model_view * vec4(vert_position, 1.0f);
  frag_tex_coord = vert_tex_coord;
}