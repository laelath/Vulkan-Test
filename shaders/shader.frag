#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushConsts {
    vec4 dirLight;
    vec4 dirLightColor;
} pushConsts;

layout(location = 0) in vec3 fragDir;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

#define AMBIENT vec3(0.2, 0.2, 0.2)
#define SPECULAR 25

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);

    float dirLightFactor = max(0, dot(fragNormal, pushConsts.dirLight.xyz));
    vec3 diffuse = texColor.xyz * pushConsts.dirLightColor.xyz * dirLightFactor;

    vec3 reflection = normalize(reflect(pushConsts.dirLight.xyz, fragNormal));
    float lspec = max(0.0, dot(-fragDir, reflection));
    float fspec = pow(lspec, SPECULAR);
    vec3 specular = pushConsts.dirLightColor.xyz * fspec;

    vec3 ambient = AMBIENT * texColor.xyz;

    outColor.xyz = diffuse + ambient + specular;
    //outColor.xyz = (reflection + vec3(1,1,1))/2;
    outColor.w = texColor.w;
}
