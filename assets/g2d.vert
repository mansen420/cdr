#version 450

layout(location= 0) in vec2 inPos;
layout(location= 1) in vec2 inSize;
layout(location= 2) in vec4 inColor;

layout(location= 0) out vec4 outColor;

layout(push_constant) uniform pushconst { vec2 screen; } push;

void main()
{
    vec4 br= vec4(inPos + inSize, 0.0, 1.0);
    vec4 bl= vec4(inPos + vec2(0.0, inSize.y), 0.0, 1.0);
    vec4 tr= vec4(inPos + vec2(inSize.x, 0.0), 0.0, 1.0);
    vec4 tl= vec4(inPos, 0.0, 1.0);

    vec4 pos;
    switch(gl_VertexIndex)
    {
      case 0: pos= tl; break;
      case 1: pos= tr; break;
      case 2: pos= bl; break;
      case 3: pos= tr; break;
      case 4: pos= br; break;
      case 5: pos= bl; break;
    }

    gl_Position= pos / vec4(push.screen, 1.0, 1.0) * 2.0 - 1.0;
    outColor= inColor;
}
