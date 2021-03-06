#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferData {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragDir;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragDir = normalize((ubo.view * ubo.model * vec4(inPosition, 1.0)).xyz);
    fragNormal = normalize((ubo.model * vec4(inNormal, 0.0)).xyz);
    fragTexCoord = inTexCoord;
}
