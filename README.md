HlslShaderToy
HlslShaderToy is inspired by www.shadertoy.com

ShaderToy is such an amazing website, I can learn a lot from the fabulous shaders.
But in daily work, I am more concerned about Hlsl, which is the shader language for Microsoft Direct3D API.
So it would be nice for me have a Hlsl version of ShaderToy, it will be more standalone and won't run on a browser.

```
Texture2D iChannel[4] : register( t0 );

SamplerState samLinear : register( s0 );

cbuffer cbNeverChanges : register( b0 )
{
    float3      iResolution;     // viewport resolution (in pixels)
    float       iGlobalTime;     // shader playback time (in seconds)
    float       iChannelTime[4]; // channel playback time (in seconds)
    float4      iMouse;          // mouse pixel coords. xy: current (if MLB down), zw: click
    float4      iDate;           // (year, month, day, time in seconds)
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};
```