// e:\__svn_pool\HlslShaderToy\media\photo_4.jpg
 
float4 main( float4 pos : SV_POSITION) : SV_Target
{
    return iChannel[0].Sample( smooth, pos.xy / iResolution );
}