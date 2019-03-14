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

#include "DXApplication.h"

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class Renderer : public DXApplication
{
public:
    Renderer(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

private:
    static const UINT FrameCount = 2;

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_rect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    void LoadPipeline();
    void CreateFactory(_Out_ ComPtr<IDXGIFactory4> &factory);
    void CreateDevice(_In_ ComPtr<IDXGIFactory4> &factory, _Out_ ComPtr<ID3D12Device>& device);
    void CreateCommandQueue(_In_ ComPtr<ID3D12Device>& device, _Out_ ComPtr<ID3D12CommandQueue>& commandQueue);
    void CreateSwapChain(_In_ ComPtr<IDXGIFactory4> &factory, _Out_ ComPtr<IDXGISwapChain3>& swapChain, _Out_ UINT& frameIndex);
    void CreateRTVDescriptorHeap(_In_ ComPtr<ID3D12Device>& device, _Out_ ComPtr<ID3D12DescriptorHeap>& rtvHeap, _Out_ UINT& rtvDescriptorSize);
    void CreateRTVs(_In_ ComPtr<ID3D12Device>& device, _In_ ComPtr<ID3D12DescriptorHeap>& rtvHeap, _In_ ComPtr<IDXGISwapChain3>& swapChain, UINT& rtvDescriptorSize, _Out_ ComPtr<ID3D12Resource>* renderTargets);

    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
};
