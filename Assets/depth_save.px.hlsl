struct PixelInput
{
    float3 color : COLOR;
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PixelOutput
{
    float attachment0 : SV_Target0;
};

PixelOutput main(PixelInput pixelInput)
{
    PixelOutput output;
    output.attachment0 = pixelInput.position.z;
    return output;
}
