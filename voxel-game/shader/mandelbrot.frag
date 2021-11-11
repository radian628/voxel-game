#version 430

in vec3 in_position;
out vec4 fragColor;

void main() {
  vec2 c = in_position.xy;
  vec2 z = vec2(0.0);
  float i = 0.0;
  for (i = 0.0; i < 32.0; i++) {
    z = vec2(
      z.x * z.x - z.y * z.y,
      2 * z.x * z.y
    ) + c;
    if (dot(z,z) > 4.0) break;
  }
  fragColor = vec4(vec3(i / 32.0), 1.0);
}