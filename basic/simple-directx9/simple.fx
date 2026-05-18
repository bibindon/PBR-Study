float4x4 g_matWorldViewProj;
float4x4 g_matWorld;
float3 g_cameraPositionW;
float4 g_materialDiffuse;
float4 g_pbrBaseColorFactor;
bool g_hasDiffuseTexture;
bool g_enableSrgbToLinear;
bool g_enableLinearToSrgb;
float3 g_lightDirectionW;
float4 g_lightColor;
float g_lightPower;
float g_pbrRoughness;
float g_pbrMetallic;
float g_envReflectionIntensity;

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

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0f) + 1.0f;
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0001f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;

    float denom = NdotV * (1.0f - k) + k;
    return NdotV / max(denom, 0.0001f);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));

    float ggxV = GeometrySchlickGGX(NdotV, roughness);
    float ggxL = GeometrySchlickGGX(NdotL, roughness);

    return ggxV * ggxL;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - saturate(cosTheta), 5.0f);
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

float4 PbrDirectLightPixelShader(float3 posWorld  : TEXCOORD0,
                                 float3 normWorld : TEXCOORD1,
                                 float2 uv        : TEXCOORD2) : COLOR
{
    float3 albedo = GetPbrAlbedo(uv);

    float3 N = normalize(normWorld);
    float3 L = normalize(g_lightDirectionW);
    float3 V = normalize(g_cameraPositionW - posWorld);
    float3 H = normalize(L + V);
    float3 R = reflect(-V, N);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));

    float roughness = max(g_pbrRoughness, 0.04f);
    float metallic = saturate(g_pbrMetallic);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo, metallic);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(saturate(dot(H, V)), F0);

    float3 numerator = D * G * F;
    float denominator = 4.0f * max(NdotV, 0.0001f) * max(NdotL, 0.0001f);
    float3 specularBRDF = numerator / max(denominator, 0.0001f);

    float3 kS = F;
    float3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;

    float3 diffuseBRDF = kD * albedo * (1.0f / PI);
    float3 radiance = g_lightColor.rgb * g_lightPower;
    float3 directColor = (diffuseBRDF + specularBRDF) * radiance * NdotL;

    float3 envColor = texCUBE(EnvSamp, R).rgb;
    if (g_enableSrgbToLinear)
    {
        envColor = SrgbToLinear(envColor);
    }

    float3 envF = FresnelSchlick(saturate(dot(N, V)), F0);
    float envSpecularStrength = lerp(0.1f, 1.0f, metallic);
    float3 envSpecular = envColor * envF * envSpecularStrength * g_envReflectionIntensity;
    float3 color = directColor + envSpecular;

    if (g_enableLinearToSrgb)
    {
        color = LinearToSrgb(color);
    }

    return float4(color, g_pbrBaseColorFactor.a);
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

technique PbrDirectLightTechnique
{
    pass P0
    {
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PbrDirectLightPixelShader();
    }
}
