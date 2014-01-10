shadertoy-dx11
==

shadertoy-dx11 is inspired by www.shadertoy.com

ShaderToy is such an amazing website, I enjoy learning from the fabulous shaders.   
In daily work, I care more about Hlsl, which is the shader language for Microsoft Direct3D API.   
So it would be nice for me to have a standalone Hlsl version of ShaderToy.   
Certainly it won't run in a browser since there is no WebD3D.   

The usage is quite straightforward:
```
./bin/shadertoy-dx11.exe ./samples/HelloWorld.toy
```

Take HelloWorld.hlsl as an example, which does nothing except showing textures[0] aka "photo_4.jpg" on screen.   
```glsl
// photo_4.jpg

float4 main( float4 pos : SV_POSITION) : SV_Target
{
    return textures[0].Sample( smooth, pos.xy / resolution );
}
```
![screenshot](/doc/helloworld.png "./bin/shadertoy-dx11.exe ./samples/HelloWorld.toy")

If you have previous experience with Hlsl coding, then you must be wondering WTF is ***smooth*** and ***textures[0]***   
And how is ***photo_4.jpg*** mapping to graphics pipeline?

Take it easy, the secrect is that following sentences are automatilly added to the shader you provide    
```glsl
Texture2D backbuffer : register( t0 );
Texture2D textures[#n] : register( t1 ); // the value of #n is assigned by the available textures

SamplerState smooth : register( s0 );
SamplerState blocky : register( s1 );
SamplerState mirror : register( s2 );

cbuffer CBOneFrame : register( b0 )
{
    float2     resolution;     // viewport resolution (in pixels)
    float      time;           // shader playback time (in seconds)
    float      aspect;         // cached aspect of viewport
    float4     mouse;          // mouse pixel coords. xy: current (if MLB down), zw: click
    float4     date;           // (year, month, day, time in seconds)
};
```
