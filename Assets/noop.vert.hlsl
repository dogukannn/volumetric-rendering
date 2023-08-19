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
    VertexOutput output;
    output.position = float4(vertexInput.inPos, 1.0);
    output.uv = vertexInput.inUV;
    output.color = vertexInput.inColor;
    output.normal = vertexInput.inNormal;
    return output;
}
