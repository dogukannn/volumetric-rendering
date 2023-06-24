cbuffer cb : register(b0)
{
    row_major float4x4 projectionMatrix : packoffset(c0);
    row_major float4x4 modelMatrix : packoffset(c4);
    row_major float4x4 viewMatrix : packoffset(c8);
};

Texture2D g_texture : register(t1);
SamplerState s1 : register(s0);

struct VertexInput
{
    float3 inPos : POSITION;
    float3 inNormal : NORMAL;
    float3 inColor : COLOR;
    float2 inUV : TEXCOORD;
};

struct VertexOutput
{
    float3 color : COLOR;
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VertexOutput main(VertexInput vertexInput)
{
    float3 inColor = vertexInput.inColor;
    float3 inPos = vertexInput.inPos;
    float4 position = mul(float4(inPos, 1.0f), mul(modelMatrix, mul(viewMatrix, projectionMatrix)));

    VertexOutput output;
    output.position = position;
    output.uv = vertexInput.inUV;
    //output.position = float4(inPos, 1.0f);
    output.color = inColor;
    output.normal = vertexInput.inNormal;
    return output;
}
