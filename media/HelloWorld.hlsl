
float4 main( PS_Input input) : SV_Target
{
    return iChannel[0].Sample( samLinear, input.tex );
}