#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushConsts {
    vec4 dirLight;
    vec4 dirLightColor;
} pushConsts;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);
    float dirLightFactor = max(0, dot(fragNormal, pushConsts.dirLight.xyz));
    outColor.xyz = texColor.xyz * pushConsts.dirLightColor.xyz * dirLightFactor;
    outColor.w = texColor.w;
}
