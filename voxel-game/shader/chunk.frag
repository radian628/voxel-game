#version 430

precision highp float;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

in vec3 normal;
flat in uint material;

out vec4 fragColor;

//layout(location = 4) uniform sampler2D shadowMap1;
//layout(location = 5) uniform sampler2D shadowMap2;
//layout(location = 6) uniform vec4 lightDir;

void main() {
  float brightness = max(dot(normal, normalize(vec3(1.0, 2.0, 3.0))), 0.15);
  fragColor = vec4(brightness * hsv2rgb(vec3(float(1) * 0.3, 1.0, 1.0)) * (1.0 - gl_FragCoord.z / gl_FragCoord.w / 300.0), 1.0);
  //fragColor = vec4(normal * 0.5 + 0.5, 1.0);
}