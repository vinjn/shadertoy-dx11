HlslShaderToy
HlslShaderToy is inspired by www.shadertoy.com

ShaderToy is such an amazing website, I enjoy learning from the fabulous shaders.   
In daily work, I care more about Hlsl, which is the shader language for Microsoft Direct3D API.   
So it would be nice for me to have a Hlsl version of ShaderToy, it a standalone application and won't run in a browser.   

Usage is very simple:
```
bin/HlslShaderToy.exe ../media/HelloWorld.hlsl
```

The following sentences are automatilly added to the shader you provide    
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

Take HelloWorld.hlsl as an example, which does nothing except showing a image(iChannel[0]) on screen.   
```
float4 main( PS_Input input) : SV_Target
{
    return iChannel[0].Sample( samLinear, input.tex );
}
```
