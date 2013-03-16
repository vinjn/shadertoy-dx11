HlslShaderToy
==

HlslShaderToy is inspired by www.shadertoy.com

ShaderToy is such an amazing website, I enjoy learning from the fabulous shaders.   
In daily work, I care more about Hlsl, which is the shader language for Microsoft Direct3D API.   
So it would be nice for me to have a standalone Hlsl version of ShaderToy.   
Certainly it won't run in a browser since there is no WebD3D.   

The usage is quite straightforward:
```
bin/HlslShaderToy.exe ../media/HelloWorld.hlsl
```

Take HelloWorld.hlsl as an example, which does nothing except showing a image(iChannel[0]) on screen.   
```glsl
// ../media/photo_4.jpg

float4 main( PS_Input input) : SV_Target
{
    return iChannel[0].Sample( smooth, input.tex );
}
```

If you have previous experience with Hlsl coding, then you must be wondering WTF is smooth and iChannel[0]!!!   

Take it easy, the secrect is that following sentences are automatilly added to the shader you provide    
```glsl
Texture2D iChannel[4] : register( t0 );

SamplerState smooth : register( s0 );
SamplerState blocky : register( s1 );

cbuffer cbNeverChanges : register( b0 )
{
    float2     iResolution;     // viewport resolution (in pixels)
    float      iGlobalTime;     // shader playback time (in seconds)
    float      pad;             // padding
    float4     iChannelTime;   // channel playback time (in seconds)
    float4     iMouse;          // mouse pixel coords. xy: current (if MLB down), zw: click
    float4     iDate;           // (year, month, day, time in seconds)
};

```
