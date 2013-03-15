// https://www.shadertoy.com/view/4dfGzn
// texture2D(iChannel[0] -> iChannel[0].Sample(samLinear
// gl_FragCoord -> input.pos

float4 main( float4 pos : SV_POSITION ) : SV_Target
{
    float2 q = pos.xy / iResolution;
    float2 uv = 0.5 + (q-0.5)*(0.9 + 0.1*sin(0.2*iGlobalTime));

    float3 oricol = iChannel[0].Sample(samLinear, float2(q.x,1.0-q.y) ).xyz;
    float3 col;

    col.r = iChannel[0].Sample(samLinear, float2(uv.x+0.003,-uv.y)).x;
    col.g = iChannel[0].Sample(samLinear, float2(uv.x+0.000,-uv.y)).y;
    col.b = iChannel[0].Sample(samLinear, float2(uv.x-0.003,-uv.y)).z;

    col = clamp(col*0.5+0.5*col*col*1.2,0.0,1.0);

    col *= 0.5 + 0.5*16.0*uv.x*uv.y*(1.0-uv.x)*(1.0-uv.y);

    col *= float3(0.95,1.05,0.95);

    col *= 0.9+0.1*sin(10.0*iGlobalTime+uv.y*1000.0);

    col *= 0.99+0.01*sin(110.0*iGlobalTime);

    float comp = smoothstep( 0.2, 0.7, sin(iGlobalTime) );
    col = lerp( col, oricol, clamp(-2.0+2.0*q.x+3.0*comp,0.0,1.0) );

    return float4(col,1.0);
}