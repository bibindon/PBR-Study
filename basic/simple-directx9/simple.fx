float4x4 g_matWorldViewProj;
float4x4 g_matWorld;
float4 g_materialDiffuse;
float4 g_pbrBaseColorFactor;
bool g_hasDiffuseTexture;
bool g_enableSrgbToLinear;
bool g_enableLinearToSrgb;
float3 g_lightDirectionW;
float4 g_lightColor;
float g_lightPower;

#define PI 3.14159265f

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

float3 SrgbToLinear(float3 c)
{
    return pow(saturate(c), 2.2f);
}

float3 LinearToSrgb(float3 c)
{
    return pow(saturate(c), 1.0f / 2.2f);
}

float3 GetBaseColorTexture(float2 uv)
{
    if (g_hasDiffuseTexture)
    {
        float3 textureColor = tex2D(DiffuseSamp, uv).rgb;

        if (g_enableSrgbToLinear)
        {
            textureColor = SrgbToLinear(textureColor);
        }

        return textureColor;
    }

    return float3(1.0f, 1.0f, 1.0f);
}

float3 GetPbrBaseColorFactor()
{
    return g_pbrBaseColorFactor.rgb;
}

float3 GetPbrAlbedo(float2 uv)
{
    float3 baseColorFactor = GetPbrBaseColorFactor();
    float3 materialColor = g_materialDiffuse.rgb;
    float3 baseColorTexture = GetBaseColorTexture(uv);
    return baseColorFactor * materialColor * baseColorTexture;
}

float4 PbrDiffusePixelShader(float3 posWorld  : TEXCOORD0,
                             float3 normWorld : TEXCOORD1,
                             float2 uv        : TEXCOORD2) : COLOR
{
    float3 albedo = GetPbrAlbedo(uv);

    float3 N = normalize(normWorld);
    float3 L = normalize(g_lightDirectionW);
    float NdotL = saturate(dot(N, L));

    float3 diffuse = albedo * (1.0f / PI) * g_lightColor.rgb * g_lightPower * NdotL;

    if (g_enableLinearToSrgb)
    {
        diffuse = LinearToSrgb(diffuse);
    }

    return float4(diffuse, g_pbrBaseColorFactor.a);
}

float4 SkyboxPixelShader(float3 posWorld : TEXCOORD0) : COLOR
{
    float3 sampleDir = normalize(posWorld);
    return float4(texCUBE(EnvSamp, sampleDir).rgb, 1.0f);
}

technique SkyboxTechnique
{
    pass P0
    {
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 SkyboxPixelShader();
    }
}

technique PbrDiffuseTechnique
{
    pass P0
    {
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PbrDiffusePixelShader();
    }
}
