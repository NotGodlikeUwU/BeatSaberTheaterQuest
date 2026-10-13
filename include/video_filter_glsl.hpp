#pragma once
namespace theater {
inline constexpr char VideoFilterVertex[] = R"GLSL(#version 300 es
precision highp float;
out vec2 uv;
void main() {
    vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));
    uv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);
}
)GLSL";
inline constexpr char VideoFilterFragment[] = R"GLSL(#version 300 es
precision highp float;
uniform sampler2D video;
uniform vec4 colorA;
uniform vec4 colorB;
uniform int oval;
in vec2 uv;
out vec4 outputColor;
void main() {
    vec4 sampleColor=texture(video,uv);
    // Order, additive brightness, Rec.601 luma, Rodrigues hue rotation and
    // vignette equations follow the PC bundle's VideoShader pixel program.
    // The unexposed shadows/midtones/highlights uniforms retain their defaults.
    vec3 c=sampleColor.rgb*colorB.x;
    c=colorB.y>0.0?pow(max(c,vec3(0.0)),vec3(1.0/colorB.y)):step(vec3(1.0),c);
    c=(c-0.5)*colorA.y+0.5;
    c+=colorA.x-1.0;
    float luminance=dot(c,vec3(0.299,0.587,0.114));
    c=mix(vec3(luminance),c,colorA.z);
    vec3 axis=vec3(0.57735026919);
    float angle=colorA.w*6.28318530718;
    c=c*cos(angle)+cross(axis,c)*sin(angle)+axis*dot(axis,c)*(1.0-cos(angle));
    vec2 p=abs(uv*2.0-1.0);
    float distanceFromCenter=mix(length(p),max(p.x,p.y),max(p.x,p.y));
    float edge=colorB.w>0.0?smoothstep(0.0,colorB.w,colorB.z-distanceFromCenter):step(distanceFromCenter,colorB.z);
    if(oval!=0) {
        float radius=clamp(colorB.z,0.05,1.0);
        vec2 q=(uv-0.5)*sqrt(radius)+0.5;
        float d=q.x*q.y*(1.0-q.x)*(1.0-q.y)-(1.0-radius)*0.0625;
        edge=colorB.w>0.0?smoothstep(0.0,colorB.w*0.0125,d):step(0.0,d);
    }
    outputColor=vec4(clamp(c*edge,0.0,1.0),sampleColor.a*edge);
}
)GLSL";
}
