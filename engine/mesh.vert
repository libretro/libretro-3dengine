#version 450
layout(location = 0) in vec3 aVertex;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

layout(push_constant) uniform PC { mat4 model; mat4 mvp; } pc;

layout(location = 0) out vec2 vTex;
layout(location = 1) out vec4 vNormal;
layout(location = 2) out vec4 vPos;

void main()
{
   vec4 v      = vec4(aVertex, 1.0);
   gl_Position = pc.mvp * v;
   vTex        = aTex;
   vPos        = pc.model * v;
   vNormal     = pc.model * vec4(aNormal, 0.0);
}
