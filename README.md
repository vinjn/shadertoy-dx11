shadertoy-dx11
==

Introduction
--
shadertoy-dx11 is inspired by www.shadertoy.com

ShaderToy is such an amazing website, I enjoy learning from the fabulous shaders.   
In daily work, I care more about Hlsl, which is the shader language for Microsoft Direct3D API.
So it would be nice for me to have a standalone Hlsl version of ShaderToy.
Certainly it won't run in a browser since there is no WebD3D :)

Basic
--
The usage is quite straightforward:
```
./bin/shadertoy-dx11.exe ./samples/HelloWorld.toy
```

Take HelloWorld.hlsl as an example, which does nothing except showing textures[0] aka "photo_4.jpg" on screen.   
```glsl
// demonstrate the use of local image
// photo_4.jpg
// ducky.png
 
float4 main(float4 pos : SV_POSITION) : SV_Target
{
    float4 clr0 = textures[0].Sample( smooth, pos.xy / resolution );
    float4 clr1 = textures[1].Sample( blocky, pos.xy / resolution );

    return lerp(clr0, clr1, mouse.x / resolution.x);
}
```
![screenshot](/doc/helloworld.png "./bin/shadertoy-dx11.exe ./samples/HelloWorld.toy")

If you have previous experience with Hlsl coding, then you must be wondering WTF is `smooth` and `blocky`. 
And how can be `photo_4.jpg` and `ducky.png` reference by `textures[0]` and `textures[1]`?

Take it easy, the secret behind is that following lines are automatilly added to the shader you write:    
```glsl
Texture2D backbuffer : register( t0 );
Texture2D textures[#n] : register( t1 ); // the exact number of #n is automatically calculated by the shadertoy-dx11

SamplerState smooth : register( s0 );
SamplerState blocky : register( s1 );
SamplerState mirror : register( s2 );

cbuffer CBOneFrame : register( b0 )
{
    float2     resolution;     // viewport resolution (in pixels)
    float      time;           // shader playback time (in seconds)
    float      aspect;         // cached aspect of viewport
    float4     mouse;          // mouse pixel coords. xy: current (if MLB down), zw: click (TODO:)
    float4     date;           // (year, month, day, time in seconds) (TODO:)
};
```

Advanced
--
shadertoy-dx11 turns various input source into texture:
* relative image path, as `// photo_4.jpg`.
* absolute image path, as `// C:/photo_4.jpg`.
* image on Internet, as `// https://f.cloud.github.com/assets/558657/590903/5b36a0e0-c9f3-11e2-93f0-743a1469c0d9.png`, also refer to `samplers/HelloWorldUrl.toy`.
* physical camera devices, as `// camera`, also refer to `samplers/HelloWorldCamera.toy`.
