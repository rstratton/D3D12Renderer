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

#pragma once

#include "SceneObject.h"
#define _USE_MATH_DEFINES
#include <math.h>

#define MSAA_SAMPLE_COUNT 8

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

static const int MAX_LIGHTS = 10;

struct DirectionalLight {
    XMFLOAT4 direction;
    XMFLOAT4 color;
};

class Camera {
public:
    XMFLOAT3 position;
    float azimuthalAngle;
    float polarAngle;
    
    const float POS_DELTA = 0.10f;
    const float ROT_DELTA = 2.f;

    Camera(XMFLOAT3 pos) : position(pos), azimuthalAngle(0.f), polarAngle(90.f) {}

    XMFLOAT4X4 getViewMatrix() {
        XMVECTOR pos = XMLoadFloat3(&position);
        XMFLOAT3 lookDirection = getLookDirection();
        XMVECTOR lookDir = XMLoadFloat3(&lookDirection);
        XMFLOAT4X4 result;
        XMStoreFloat4x4(&result, XMMatrixLookAtLH(pos, pos + lookDir, { 0.f, 1.f, 0.f }));
        return result;
    }

    XMFLOAT3 getLookDirection() {
        float radiansPolar = (polarAngle * M_PI) / 180.f;
        float radiansAzimuthal = (azimuthalAngle * M_PI) / 180.f;
        float cosPolar = cos(radiansPolar);
        float sinPolar = sin(radiansPolar);
        float cosAzimuthal = cos(radiansAzimuthal);
        float sinAzimuthal = sin(radiansAzimuthal);

        return XMFLOAT3{
            sinPolar * cosAzimuthal,
            cosPolar,
            sinPolar * sinAzimuthal
        };
    }

    XMFLOAT3 getHorizontalLookDirection() {
        float radiansAzimuthal = (azimuthalAngle * M_PI) / 180.f;

        return XMFLOAT3{
            cos(radiansAzimuthal),
            0.f,
            sin(radiansAzimuthal)
        };
    }

    void translateUp() {
        position.y += POS_DELTA;
    }

    void translateDown() {
        position.y -= POS_DELTA;
    }

    void translateForward() {
        XMVECTOR pos = XMLoadFloat3(&position);
        XMFLOAT3 lookDirection = getHorizontalLookDirection();
        XMVECTOR lookDir = XMLoadFloat3(&lookDirection);
        XMVECTOR newPos = pos + (lookDir * POS_DELTA);
        XMStoreFloat3(&position, newPos);
    }

    void translateBackward() {
        XMVECTOR pos = XMLoadFloat3(&position);
        XMFLOAT3 lookDirection = getHorizontalLookDirection();
        XMVECTOR lookDir = XMLoadFloat3(&lookDirection);
        XMVECTOR newPos = pos - (lookDir * POS_DELTA);
        XMStoreFloat3(&position, newPos);
    }

    void translateLeft() {
        XMVECTOR pos = XMLoadFloat3(&position);
        XMFLOAT3 lookDirection = getLookDirection();
        XMVECTOR lookDir = XMLoadFloat3(&lookDirection);
        XMVECTOR up = { 0.f, 1.f, 0.f };
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, lookDir));
        XMVECTOR newPos = pos - (right * POS_DELTA);
        XMStoreFloat3(&position, newPos);
    }

    void translateRight() {
        XMVECTOR pos = XMLoadFloat3(&position);
        XMFLOAT3 lookDirection = getLookDirection();
        XMVECTOR lookDir = XMLoadFloat3(&lookDirection);
        XMVECTOR up = { 0.f, 1.f, 0.f };
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, lookDir));
        XMVECTOR newPos = pos + (right * POS_DELTA);
        XMStoreFloat3(&position, newPos);
    }

    void rotateLeft() {
        azimuthalAngle += ROT_DELTA;
        constrainAzimuthalAngle();
    }

    void rotateRight() {
        azimuthalAngle -= ROT_DELTA;
        constrainAzimuthalAngle();
    }

    void constrainAzimuthalAngle() {
        if (azimuthalAngle < 0.f) {
            azimuthalAngle += 360.f;
        } else if (azimuthalAngle > 360.f) {
            azimuthalAngle -= 360.f;
        }
    }

    void rotateUp() {
        polarAngle -= ROT_DELTA;
        constrainPolarAngle();
    }

    void rotateDown() {
        polarAngle += ROT_DELTA;
        constrainPolarAngle();
    }

    void constrainPolarAngle() {
        if (polarAngle <= 0.f) {
            polarAngle = ROT_DELTA;
        } else if (polarAngle >= 180.f) {
            polarAngle = 180.f - ROT_DELTA;
        }
    }
};

class Renderer : public DXApplication
{
public:
    Renderer(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);

    struct Constants {
        XMFLOAT4X4 view;
        XMFLOAT4X4 proj;
    };

    static const UINT FrameCount = 2;

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_rect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12Resource> m_depthStencilBuffer;
    ComPtr<ID3D12DescriptorHeap> m_depthStencilDescriptorHeap;
    ComPtr<ID3D12Resource> m_textureMSAA;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    UINT m_rtvDescriptorSize;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    // Constants
    Constants m_constants;
    ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_pConstantBufferData;

    Camera m_camera;

    // Lights
    DirectionalLight m_lights[MAX_LIGHTS];
    int m_numLights;
    ComPtr<ID3D12Resource> m_lightBuffer;
    UINT8* m_pLightBufferData;

    std::vector<SceneObject> m_sceneObjects;

    void LoadPipeline();
    void CreateFactory(_Out_ ComPtr<IDXGIFactory4> &factory);
    void CreateDevice(_In_ ComPtr<IDXGIFactory4> &factory, _Out_ ComPtr<ID3D12Device>& device);
    void CreateCommandQueue(_In_ ComPtr<ID3D12Device>& device, _Out_ ComPtr<ID3D12CommandQueue>& commandQueue);
    void CreateSwapChain(_In_ ComPtr<IDXGIFactory4> &factory, _Out_ ComPtr<IDXGISwapChain3>& swapChain, _Out_ UINT& frameIndex);
    void CreateRTVDescriptorHeap(_In_ ComPtr<ID3D12Device>& device, _Out_ ComPtr<ID3D12DescriptorHeap>& rtvHeap, _Out_ UINT& rtvDescriptorSize);
    void CreateDepthStencilDescriptorHeap(_In_ ComPtr<ID3D12Device>& device, _Out_ ComPtr<ID3D12DescriptorHeap>& dsHeap);
    void CreateRTVs(_In_ ComPtr<ID3D12Device>& device, _In_ ComPtr<ID3D12DescriptorHeap>& rtvHeap, _In_ ComPtr<IDXGISwapChain3>& swapChain, UINT& rtvDescriptorSize, _Out_ ComPtr<ID3D12Resource>* renderTargets);
    void CreateDepthStencilBuffer(_In_ ComPtr<ID3D12Device>& device, _In_ ComPtr<ID3D12DescriptorHeap>& dsHeap, _Out_ ComPtr<ID3D12Resource>& depthStencilBuffer);

    void CreateMSAARenderTarget(ComPtr<ID3D12Device>& device, ComPtr<ID3D12Resource>& textureMSAA);

    void LoadAssets();
    void CreateCommandList(_In_ ComPtr<ID3D12Device>& device, _In_ ComPtr<ID3D12PipelineState>& pipelineState, _In_ ComPtr<ID3D12CommandAllocator>& commandAllocator, _Out_ ComPtr<ID3D12GraphicsCommandList>& commandList);
    void CreateFence(_In_ ComPtr<ID3D12Device>& device, _Out_ ComPtr<ID3D12Fence>& fence, _Out_ HANDLE& fenceEvent, _Out_ UINT64& fenceValue);
    void CreateRootSignature(_In_ ComPtr<ID3D12Device>& device, _Out_ ComPtr<ID3D12RootSignature>& rootSignature);
    D3D_ROOT_SIGNATURE_VERSION GetRootSignatureVersion(_In_ const ComPtr<ID3D12Device>& device);
    void CreatePSO(_In_ ComPtr<ID3D12Device>& device, _In_ ComPtr<ID3D12RootSignature>& rootSignature, _Out_ ComPtr<ID3D12PipelineState>& pipelineState);
    void CompileShader(_In_ const LPCWSTR fname, _In_ const LPCSTR entryPoint, _In_ const LPCSTR target, _Out_ ComPtr<ID3DBlob>& compiledShader);
    void CreateGlobalConstants(_In_ const ComPtr<ID3D12Device>& device);

    void PopulateCommandList();
    void WaitForPreviousFrame();
};
