cbuffer cb : register(b0)
{
    row_major float4x4 mvp : packoffset(c0);
    row_major float4x4 inverseVP : packoffset(c4);
    float3 eye : packoffset(c8.x);
    float time : packoffset(c8.w);
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


float rand(float3 p) 
{
    return frac(sin(dot(p, float3(12.345, 67.89, 412.12))) * 42123.45) * 2.0f - 1.0f;
    //return frac(sin(dot(p,
    //                     float3(12.9898,78.233,24.52345)))*
    //    43758.5453123) * 2.0f - .9f;
}

float valueNoise(float3 p)
{
    float3 u = floor(p);
    float3 v = frac(p);
    float3 s = smoothstep(0.0, 1.0, v);
    
    
    float a = rand(u);
    float b = rand(u + float3(1.0, 0.0, 0.0));
    float c = rand(u + float3(0.0, 1.0, 0.0));
    float d = rand(u + float3(1.0, 1.0, 0.0));
    float e = rand(u + float3(0.0, 0.0, 1.0));
    float f = rand(u + float3(1.0, 0.0, 1.0));
    float g = rand(u + float3(0.0, 1.0, 1.0));
    float h = rand(u + float3(1.0, 1.0, 1.0));
    
    return lerp(lerp(lerp(a, b, s.x), lerp(c, d, s.x), s.y),
               lerp(lerp(e, f, s.x), lerp(g, h, s.x), s.y),
               s.z);
}

float fbm(float3 p)
{
    float3 q = p - float3(0.5, 0.0, 0.0) * time;
	int numOctaves = 8;
    float weight = 0.7;
    float ret = 0.0;
    
    for (int i = 0; i < numOctaves; i++)
    {
        ret += weight * valueNoise(q);
        q *= 2.0;
        weight *= 0.5;
    }
    
    return clamp(ret, 0.0, 1.0);
}

float4 volumetricMarch(float3 enter, float3 exit)
{
    float3 ro = eye;
    float3 rd = normalize(enter - eye);

    float depth = 0.0;
    float4 color = float4(0., 0., 0., 0.);

    float minDistance = length(eye - enter);
    float maxDistance = length(eye - exit);
    
    for (int i = 0; i < 250; i++)
    {
        float3 p = ro + depth * rd;
        float curDist = length(p - ro);
        if(curDist > maxDistance)
        {
            break;
        }
        float density = 0;
        if(curDist > minDistance)
        {
            density = fbm(p * 0.9);
			density *= valueNoise(p * 0.4);
        }
        
        if(density > 1e-3)
        {
            float4 c = float4(lerp(float3(1.0, 1.0, 1.0), float3(0.0, 0.0, 0.0), density), density);
            c.a *= 0.5;
            c.rgb *= c.a;
            color += c * (1.0 - color.a);
        }
        
        depth += max(0.05, 0.02 * depth);
    }
    
    return float4(clamp(color.rgb, 0.0, 1.0), color.a);
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
    if (enterDepth <= 0.0001)
    {
        discard;
    }
    float4 cloudColor = volumetricMarch(worldPosEnter, worldPosExit);
    //float4 cloudColor = float4(worldPosEnter.xyz, 1.0f);
    //cloudColor.x = sin(time);
    output.attachment0 = float4(cloudColor.xyzw);

    return output;
}
