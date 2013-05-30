HlslShaderToy
==

HlslShaderToy is inspired by www.shadertoy.com

ShaderToy is such an amazing website, I enjoy learning from the fabulous shaders.   
In daily work, I care more about Hlsl, which is the shader language for Microsoft Direct3D API.   
So it would be nice for me to have a standalone Hlsl version of ShaderToy.   
Certainly it won't run in a browser since there is no WebD3D.   

The usage is quite straightforward:
```
./bin/HlslShaderToy.exe ./media/HelloWorld.toy
```

Take HelloWorld.hlsl as an example, which does nothing except showing textures[0] aka "../media/photo_4.jpg" on screen.   
```glsl
// ../media/photo_4.jpg

float4 main( float4 pos : SV_POSITION) : SV_Target
{
    return textures[0].Sample( smooth, input.tex );
}
```
![screenshot](/doc/helloworld.png "./bin/HlslShaderToy.exe ./media/HelloWorld.toy")

If you have previous experience with Hlsl coding, then you must be wondering WTF is smooth and textures[0]!!!   
And how is "../media/photo_4.jpg" working?

Take it easy, the secrect is that following sentences are automatilly added to the shader you provide    
```glsl
Texture2D textures[1] : register( t0 );
Texture2D backbuffer : register( t1 );

SamplerState smooth : register( s0 );
SamplerState blocky : register( s1 );
SamplerState mirror : register( s2 );

cbuffer CBOneFrame : register( b0 )
{
    float2     resolution;     // viewport resolution (in pixels)
    float      time;           // shader playback time (in seconds)
    float      aspect;         // cached aspect of viewport"
    float4     mouse;          // mouse pixel coords. xy: current (if MLB down), zw: click
    float4     date;           // (year, month, day, time in seconds)
};
```
