#version 450
/* 0 = model viewer, 1 = model viewer + discard hack, 2 = scene walker. */
layout(constant_id = 0) const int MODE = 0;

layout(location = 0) in vec2 vTex;
layout(location = 1) in vec4 vNormal;
layout(location = 2) in vec4 vPos;

layout(set = 0, binding = 0) uniform Draw
{
   vec4 lightPos;
   vec4 lightAmbient;
   vec4 eyePos;
   vec4 mtlAmbient;
   vec4 mtlDiffuse;
   vec4 mtlSpecular;
   vec4 mtlParams; /* x = specular power, y = alpha mod */
} u;

layout(set = 1, binding = 0) uniform sampler2D sDiffuse;
layout(set = 1, binding = 1) uniform sampler2D sAmbient;

layout(location = 0) out vec4 outColor;

void main()
{
   vec4 colorDiffuseFull = texture(sDiffuse, vTex);
   vec4 colorAmbientFull = texture(sAmbient, vTex);

   if (MODE == 1 && colorDiffuseFull.a < 0.5)
      discard;

   vec3 lightDir = (MODE == 2) ? normalize(vPos.xyz - u.lightPos.xyz) : u.lightPos.xyz;

   vec3 colorDiffuse = mix(u.mtlDiffuse.rgb, colorDiffuseFull.rgb, vec3(colorDiffuseFull.a));
   vec3 colorAmbient = mix(u.mtlAmbient.rgb, colorAmbientFull.rgb, vec3(colorAmbientFull.a));

   vec3 normal       = normalize(vNormal.xyz);
   float directivity = dot(lightDir, -normal);

   vec3 diffuse = colorDiffuse * clamp(directivity, 0.0, 1.0);
   vec3 ambient = colorAmbient * u.lightAmbient.rgb;

   vec3 modelToFace = (MODE == 2) ? normalize(u.eyePos.xyz - vPos.xyz) : normalize(-vPos.xyz);
   float specularity = pow(clamp(dot(modelToFace, reflect(lightDir, normal)), 0.0, 1.0), u.mtlParams.x);
   vec3 specular = u.mtlSpecular.rgb * specularity;

   float alpha = (MODE == 1) ? u.mtlParams.y : u.mtlParams.y * colorDiffuseFull.a;
   outColor = vec4(diffuse + ambient + specular, alpha);
}
