#version 150

in vec2 frag_tex_coord;
out vec4 color; 
uniform sampler2D tex_img;

void main() {
  color = texture(tex_img, frag_tex_coord);
}