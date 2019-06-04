//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

static const int SHADER_FLAGS_HAS_TEXTURE = 1 << 0;

cbuffer GlobalConstants : register(b0) {
    float4x4 view;
    float4x4 proj;
}

cbuffer SceneObjectConstants : register(b1) {
    float4x4 model;
};

cbuffer Light : register(b2) {
    float3 direction;
    float3 color;
}

cbuffer ShaderFlags : register(b3) {
    int flags;
}

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 wsPosition : TEXCOORD1;
    float4 wsNormal : TEXCOORD2;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 texCoord : TEXCOORD)
{
    PSInput result;

    float4x4 mvMatrix = mul(model, view);
    float4x4 mvpMatrix = mul(mvMatrix, proj);
    result.position = mul(float4(position, 1.f), mvpMatrix);

    result.wsPosition = mul(float4(position, 1.f), model);
    // Should use inverse transpose of model matrix for normal transformation
    result.wsNormal = normalize(mul(float4(normal, 0.f), model));
    result.texCoord = texCoord;

    return result;
}

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    // return abs(input.wsNormal);
    //float4 surfaceColor;

    //if (flags & SHADER_FLAGS_HAS_TEXTURE) {
    //    surfaceColor = g_texture.Sample(g_sampler, input.texCoord);
    //}
    //else {
    //    surfaceColor = float4(1.f, 1.f, 1.f, 0.f);
    //}

    //float normalDotLight = max(0, dot(input.wsNormal, -float4(direction, 0.f)));
    //return surfaceColor * float4(color, 0.f) * normalDotLight;

    float4 surfaceColor = float4(1.f, 1.f, 1.f, 0.f);

    float normalDotLight = max(0, dot(input.wsNormal, -float4(direction, 0.f)));
    return surfaceColor * float4(color, 0.f) * normalDotLight;
}
