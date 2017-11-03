#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform Transforms {
    mat4 model;
    mat4 view;
    mat4 proj;
} mats;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = mats.proj * mats.view * mats.model * vec4(inPosition, 0.0, 1.0);
    fragColor   = inColor;
}
