#version 430

layout(location=0) in vec4 vertexPositionIn;
layout(location=1) in vec4 normalIn;

out vec3 normal;
flat out uint material;

layout(location = 3) uniform mat4 modelViewProjection;

void main() {
    gl_Position = modelViewProjection * vec4(vertexPositionIn.xyz, 1.0);
    normal = normalIn.xyz;
    material = int(normalIn.w);
}