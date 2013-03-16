// e:\__svn_pool\HlslShaderToy\media\ducky.png
// comments

float4 main( float4 pos : SV_POSITION) : SV_Target
{
    return iChannel[0].Sample( samLinear, pos.xy / iResolution );
}