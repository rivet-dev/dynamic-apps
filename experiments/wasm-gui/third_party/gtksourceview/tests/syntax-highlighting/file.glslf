#version 140
varying vec4 color_out;
/*
  A color shader
*/
void main(void)
{
  // edit the color of the fragment
  vec4 new_color = color_out + vec4(0.4, 0.4, 0.1, 1.0);
  gl_FragColor = clamp(new_color, 0.0, 1.0);
}
