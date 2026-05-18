// 行列
float4x4 g_matWorldViewProj;
float4x4 g_matWorld;
float3 g_eyePosW;

// 環境キューブマップ
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

// VS：ワールド位置とワールド法線を渡す
void VertexShader1(float4 inPos    : POSITION,
                   float3 inNormal : NORMAL0,
                   float2 inUV     : TEXCOORD0,

                   out float4 outPos       : POSITION,
                   out float3 outPosWorld  : TEXCOORD0,
                   out float3 outNormWorld : TEXCOORD1)
{
    outPos = mul(inPos, g_matWorldViewProj);
    outPosWorld = mul(inPos, g_matWorld).xyz;
    outNormWorld = normalize(mul(inNormal, (float3x3) g_matWorld));
}

// PS：World 空間で反射を計算してキューブをサンプル
float4 PixelShader1(float3 posWorld  : TEXCOORD0,
                    float3 normWorld : TEXCOORD1) : COLOR
{
    // ピクセル→カメラ
    // このベクトルに-1をかけたらガラス玉風になる
    float3 viewWorld = normalize(posWorld - g_eyePosW);

    // 反射（World）
    float3 reflectWorld = reflect(viewWorld, normalize(normWorld));

    return float4(texCUBE(EnvSamp, reflectWorld).rgb, 1.0);
}

technique Technique1
{
    pass P0
    {
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShader1();
    }
}
