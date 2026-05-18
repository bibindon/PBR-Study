float4x4 g_matWorldViewProj;
float4x4 g_matWorld;
float3 g_eyePosW;
float4 g_materialDiffuse;
bool g_hasDiffuseTexture;

textureCUBE EnvMap;
samplerCUBE EnvSamp =
sampler_state
{
    Texture = <EnvMap>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

texture DiffuseMap;
sampler2D DiffuseSamp =
sampler_state
{
    Texture = <DiffuseMap>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};

void VertexShader1(float4 inPos    : POSITION,
                   float3 inNormal : NORMAL0,
                   float2 inUV     : TEXCOORD0,

                   out float4 outPos       : POSITION,
                   out float3 outPosWorld  : TEXCOORD0,
                   out float3 outNormWorld : TEXCOORD1,
                   out float2 outUV        : TEXCOORD2)
{
    outPos = mul(inPos, g_matWorldViewProj);
    outPosWorld = mul(inPos, g_matWorld).xyz;
    outNormWorld = normalize(mul(inNormal, (float3x3)g_matWorld));
    outUV = inUV;
}

float4 PixelShader1(float3 posWorld  : TEXCOORD0,
                    float3 normWorld : TEXCOORD1,
                    float2 uv        : TEXCOORD2) : COLOR
{
    float3 viewWorld = normalize(posWorld - g_eyePosW);
    float3 reflectWorld = reflect(viewWorld, normalize(normWorld));

    float3 baseColor = g_materialDiffuse.rgb;
    if (g_hasDiffuseTexture)
    {
        baseColor *= tex2D(DiffuseSamp, uv).rgb;
    }

    float3 reflectionColor = texCUBE(EnvSamp, reflectWorld).rgb;
    float3 finalColor = saturate(baseColor * 0.75f + reflectionColor * 0.25f);

    return float4(finalColor, g_materialDiffuse.a);
}

float4 SkyboxPixelShader(float3 posWorld : TEXCOORD0) : COLOR
{
    float3 sampleDir = normalize(posWorld - g_eyePosW);
    return float4(texCUBE(EnvSamp, sampleDir).rgb, 1.0f);
}

technique Technique1
{
    pass P0
    {
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShader1();
    }
}

technique SkyboxTechnique
{
    pass P0
    {
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 SkyboxPixelShader();
    }
}
