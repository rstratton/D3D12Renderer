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

cbuffer GlobalConstants : register(b0) {
    float4x4 view;
    float4x4 proj;
}

cbuffer SceneObjectConstants : register(b1) {
    float4x4 model;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 normal : NORMAL;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL)
{
    PSInput result;

    float4x4 mvMatrix = mul(model, view);
    float4x4 mvpMatrix = mul(mvMatrix, proj);
    result.position = mul(float4(position, 1.f), mvpMatrix);

    result.normal = float4(normal, 1.f);

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.normal;
}
