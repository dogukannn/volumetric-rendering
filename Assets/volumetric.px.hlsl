cbuffer cb : register(b0)
{
    row_major float4x4 mvp : packoffset(c0);
    row_major float4x4 inverseVP : packoffset(c4);
};

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

Texture2D <float> backCulled : register(t0);
Texture2D <float> frontCulled : register(t1);
SamplerState s1 : register(s0);

float3 WorldPosFromDepth(float depth, float2 uv) {
    float z = depth;

    float4 clipSpacePosition = float4(uv * 2.0 - 1.0, z, 1.0);
    clipSpacePosition.y *= -1.0f;
    float4 worldSpacePosition = mul(clipSpacePosition, inverseVP);
    worldSpacePosition /= worldSpacePosition.w;

    return worldSpacePosition.xyz;
}

PixelOutput main(PixelInput pixelInput)
{
    float2 uv = pixelInput.position.xy/float2(800, 600);
    int2 uvi = int2(floor(uv.x), floor(uv.y));
    //float exitDepth = frontCulled.Sample(s1, uv);
    //float enterDepth = backCulled.Sample(s1, uv);
    float exitDepth = frontCulled.Load(int3(floor(pixelInput.position.x), floor(pixelInput.position.y), 0));
    float enterDepth = backCulled.Load(int3(floor(pixelInput.position.x), floor(pixelInput.position.y), 0));
    float3 worldPosEnter = WorldPosFromDepth(enterDepth, uv);
    float3 worldPosExit = WorldPosFromDepth(exitDepth, uv);
    PixelOutput output;
    if (enterDepth > 0)
    {
        output.attachment0 = float4(worldPosEnter.x / 4.0f, worldPosEnter.y / 4.0f, worldPosEnter.z / 4.0f, 0.1f);
    }
    else
    {
        output.attachment0 = float4(0.00001f * worldPosExit.x, 0.f, 0.f, 0.f);
    }

    return output;
}
