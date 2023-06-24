struct PixelInput
{
    float3 color : COLOR;
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PixelOutput
{
    float4 attachment0 : SV_Target0;
};

Texture2D g_texture : register(t1);
SamplerState s1 : register(s0);

PixelOutput main(PixelInput pixelInput)
{
    PixelOutput output;
    output.attachment0 = g_texture.Sample(s1, pixelInput.uv);
    //output.attachment0 = float4(1.0f, 1.0f, 1.0f, 1.0f);

    //output.attachment0 = float4(pixelInput.normal + float3(0.5f), 1.0f);
    ////output.attachment0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
    return output;
}
