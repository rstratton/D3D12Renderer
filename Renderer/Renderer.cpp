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

#include "stdafx.h"
#include "Renderer.h"
#include "ObjLoader.h"

Renderer::Renderer(UINT width, UINT height, std::wstring name) :
    DXApplication(width, height, name),
    m_frameIndex(0),
    m_rtvDescriptorSize(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_rect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    // Initialize GDI+.
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    SceneObject so1;
    m_sceneObjects.push_back(so1);

    SceneObject so2;
    m_sceneObjects.push_back(so2);

    // Load scene object geometry
    ObjLoader::Load(L"Resources\\sphere.obj", &m_sceneObjects[0].m_vertices, m_sceneObjects[0].m_vertexCount);
    ObjLoader::Load(L"Resources\\dodecahedron.obj", &m_sceneObjects[1].m_vertices, m_sceneObjects[1].m_vertexCount);

    // Init scene object constants
    XMMATRIX model = XMMatrixTranslation(0.f, 0.f, 0.f);
    XMStoreFloat4x4(&m_sceneObjects[0].m_constants.model, model);

    model = XMMatrixTranslation(0.f, 0.f, 0.f);
    XMStoreFloat4x4(&m_sceneObjects[1].m_constants.model, model);

    // Initialize view and projection matrices
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(50.f), m_aspectRatio, 0.1f, 1000.0f);
    XMStoreFloat4x4(&m_constants.proj, proj);

    XMMATRIX view = XMMatrixLookAtLH(
        { 0.f, 0.f, -5.f },
        { 0.f, 0.f, 0.f },
        { 0.f, 1.f, 0.f }
    );
    XMStoreFloat4x4(&m_constants.view, view);

    // Initialize lights
    m_light = {
        { -1.f, -1.f, 1.f },
        {  0.5f, 1.f, 0.5f }
    };
}

void Renderer::OnInit()
{
    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void Renderer::LoadPipeline()
{
    ComPtr<IDXGIFactory4> factory;
    CreateFactory(factory);
    CreateDevice(factory, m_device);
    CreateCommandQueue(m_device, m_commandQueue);
    CreateSwapChain(factory, m_swapChain, m_frameIndex);
    CreateRTVDescriptorHeap(m_device, m_rtvHeap, m_rtvDescriptorSize);
    CreateDepthStencilDescriptorHeap(m_device, m_depthStencilDescriptorHeap);
    CreateMSAARenderTarget(m_device, m_textureMSAA);
    CreateRTVs(m_device, m_rtvHeap, m_swapChain, m_rtvDescriptorSize, m_renderTargets);
    CreateDepthStencilBuffer(m_device, m_depthStencilDescriptorHeap, m_depthStencilBuffer);
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void Renderer::CreateFactory(ComPtr<IDXGIFactory4> &factory) {
    UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
}

void Renderer::CreateDevice(ComPtr<IDXGIFactory4> &factory, ComPtr<ID3D12Device>& device) {
    if (m_useWarpDevice) {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device)
        ));
    } else {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device)
        ));
    }
}

void Renderer::CreateCommandQueue(ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue>& commandQueue) {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
}

void Renderer::CreateSwapChain(ComPtr<IDXGIFactory4> &factory, ComPtr<IDXGISwapChain3>& swapChain, UINT& frameIndex) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&swapChain));
    frameIndex = swapChain->GetCurrentBackBufferIndex();
}

void Renderer::CreateRTVDescriptorHeap(ComPtr<ID3D12Device>& device, ComPtr<ID3D12DescriptorHeap>& rtvHeap, UINT& rtvDescriptorSize) {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount + 1; // 1 backbuffer per frame + 1 MSAA RT
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void Renderer::CreateDepthStencilDescriptorHeap(ComPtr<ID3D12Device>& device, ComPtr<ID3D12DescriptorHeap>& dsHeap) {
    D3D12_DESCRIPTOR_HEAP_DESC dsHeapDesc = {};
    dsHeapDesc.NumDescriptors = 1;
    dsHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&dsHeapDesc, IID_PPV_ARGS(&dsHeap)));
}

void Renderer::CreateRTVs(ComPtr<ID3D12Device>& device, ComPtr<ID3D12DescriptorHeap>& rtvHeap, ComPtr<IDXGISwapChain3>& swapChain, UINT& rtvDescriptorSize, ComPtr<ID3D12Resource>* renderTargets) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

    // Create a RTV for each frame.
    for (UINT n = 0; n < FrameCount; n++) {
        ThrowIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
        device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize);
    }

    D3D12_RENDER_TARGET_VIEW_DESC  rtvDescMSAA = {};
    rtvDescMSAA.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
    rtvDescMSAA.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateRenderTargetView(m_textureMSAA.Get(), &rtvDescMSAA, rtvHandle);
}

void Renderer::CreateMSAARenderTarget(ComPtr<ID3D12Device>& device, ComPtr<ID3D12Resource>& textureMSAA) {
    D3D12_RESOURCE_DESC textureDescMSAA = {};
    textureDescMSAA.MipLevels = 1;
    textureDescMSAA.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDescMSAA.Width = m_width;
    textureDescMSAA.Height = m_height;
    textureDescMSAA.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    textureDescMSAA.DepthOrArraySize = 1;
    textureDescMSAA.SampleDesc.Count = MSAA_SAMPLE_COUNT;
    textureDescMSAA.SampleDesc.Quality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;
    textureDescMSAA.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    // Assign an optimized clear value; more efficient
    float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    CD3DX12_CLEAR_VALUE clearValue(DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM, clearColor);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDescMSAA,
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
        &clearValue,
        IID_PPV_ARGS(&textureMSAA)));
}

void Renderer::CreateDepthStencilBuffer(ComPtr<ID3D12Device>& device, ComPtr<ID3D12DescriptorHeap>& dsHeap, ComPtr<ID3D12Resource>& depthStencilBuffer) {
    // Create resource
    D3D12_RESOURCE_DESC desc = {};
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    desc.DepthOrArraySize = 1;
    desc.SampleDesc.Count = MSAA_SAMPLE_COUNT;
    desc.SampleDesc.Quality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthStencilBuffer)
    ));

    // Create resource view
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

    device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilDesc, dsHeap->GetCPUDescriptorHandleForHeapStart());
}

// Load the sample assets.
void Renderer::LoadAssets()
{
    CreateFence(m_device, m_fence, m_fenceEvent, m_fenceValue);
    CreateRootSignature(m_device, m_rootSignature);
    CreatePSO(m_device, m_rootSignature, m_pipelineState);
    CreateCommandList(m_device, m_pipelineState, m_commandAllocator, m_commandList);

    for (int i = 0; i < m_sceneObjects.size(); ++i) {
        auto& sceneObject = m_sceneObjects[i];
        sceneObject.UploadVertices(m_device);
    }

    // Create command list for recording memory uploads
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&commandList)));

    m_sceneObjects[0].LoadTexture(m_device, commandList, L"Resources\\sphere.bmp");
    m_sceneObjects[1].LoadTexture(m_device, commandList, L"Resources\\dodecahedron.bmp");

    CreateGlobalConstants(m_device);

    // Close the command list and execute it to begin the initial GPU setup.
    ThrowIfFailed(commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForPreviousFrame();
}

void Renderer::CreateCommandList(ComPtr<ID3D12Device>& device, ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12CommandAllocator>& commandAllocator, ComPtr<ID3D12GraphicsCommandList>& commandList) {
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState.Get(), IID_PPV_ARGS(&commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(commandList->Close());
}

void Renderer::CreateFence(ComPtr<ID3D12Device>& device, ComPtr<ID3D12Fence>& fence, HANDLE& fenceEvent, UINT64& fenceValue) {
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    fenceValue = 1;

    // Create an event handle to use for frame synchronization.
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

void Renderer::CreateRootSignature(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& rootSignature)
{
    CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
    CD3DX12_ROOT_PARAMETER1 rootParameters[5];

    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);

    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

    rootParameters[2].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_PIXEL);

    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    rootParameters[3].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

    rootParameters[4].InitAsConstants(1, 3, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Allow input layout and deny uneccessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, GetRootSignatureVersion(device), &signature, &error));
    ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
}

D3D_ROOT_SIGNATURE_VERSION Renderer::GetRootSignatureVersion(const ComPtr<ID3D12Device>& device) {
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    return featureData.HighestVersion;
}

void Renderer::CreatePSO(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12PipelineState>& pipelineState) {
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

    CompileShader(L"shaders.hlsl", "VSMain", "vs_5_0", vertexShader);
    CompileShader(L"shaders.hlsl", "PSMain", "ps_5_0", pixelShader);

    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    CD3DX12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.MultisampleEnable = TRUE;

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = MSAA_SAMPLE_COUNT;
    psoDesc.SampleDesc.Quality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
}

void Renderer::CompileShader(const LPCWSTR fname, const LPCSTR entryPoint, const LPCSTR target, ComPtr<ID3DBlob>& compiledShader) {
#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    // Taken from https://docs.microsoft.com/en-us/windows/desktop/direct3d11/how-to--compile-a-shader
    ID3DBlob* errorMessages;
    HRESULT hr = D3DCompileFromFile(GetAssetFullPath(fname).c_str(), nullptr, nullptr, entryPoint, target, compileFlags, 0, &compiledShader, &errorMessages);
    if (FAILED(hr)) {
        if (errorMessages) {
            OutputDebugStringA((char*)errorMessages->GetBufferPointer());
            errorMessages->Release();
        }
        if (compiledShader) {
            compiledShader->Release();
        }
        throw HrException(hr);
    }
}

void Renderer::CreateGlobalConstants(const ComPtr<ID3D12Device>& device) {
    // Create the constant buffer.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)));

    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pConstantBufferData)));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_lightBuffer)));

    ThrowIfFailed(m_lightBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pLightBufferData)));
}

// Update frame-based values.
void Renderer::OnUpdate()
{
    XMMATRIX offset = XMMatrixTranslation(0.01f, 0.0f, 0.0f);
    XMMATRIX model = XMLoadFloat4x4(&m_sceneObjects[0].m_constants.model);
    XMStoreFloat4x4(&m_sceneObjects[0].m_constants.model, model * offset);

    XMMATRIX rotation = XMMatrixRotationY(0.01f);
    model = XMLoadFloat4x4(&m_sceneObjects[1].m_constants.model);
    XMStoreFloat4x4(&m_sceneObjects[1].m_constants.model, model * rotation);
}

// Render the scene.
void Renderer::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void Renderer::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void Renderer::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_rect);

    // Indicate that MSAA texture will be set as render target
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textureMSAA.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), 2, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
    m_commandList->SetGraphicsRootConstantBufferView(2, m_lightBuffer->GetGPUVirtualAddress());

    // Upload latest global constants
    Constants constants;
    XMMATRIX view = XMMatrixTranspose(XMLoadFloat4x4(&m_constants.view));
    XMMATRIX proj = XMMatrixTranspose(XMLoadFloat4x4(&m_constants.proj));
    XMStoreFloat4x4(&constants.view, view);
    XMStoreFloat4x4(&constants.proj, proj);
    memcpy(m_pConstantBufferData, &constants, sizeof(constants));

    // Upload lighting data
    memcpy(m_pLightBufferData, &m_light, sizeof(m_light));

    for (int i = 0; i < m_sceneObjects.size(); ++i) {
        auto& sceneObject = m_sceneObjects[i];

        sceneObject.UploadConstants(m_device);

        ID3D12DescriptorHeap* ppHeaps[] = { sceneObject.m_descriptorHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        int shaderFlags = 0;

        if (sceneObject.m_hasTexture) {
            shaderFlags |= 1;
        };

        m_commandList->SetGraphicsRootDescriptorTable(1, sceneObject.m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
        m_commandList->SetGraphicsRoot32BitConstant(4, shaderFlags, 0);
        if (sceneObject.m_hasTexture) {
            CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(sceneObject.m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            m_commandList->SetGraphicsRootDescriptorTable(3, srvHandle);
        }

        // Record commands.
        m_commandList->IASetVertexBuffers(0, 1, &(sceneObject.m_vertexBufferView));
        m_commandList->DrawInstanced(sceneObject.m_vertexCount, 1, 0, 0);
    }

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textureMSAA.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE));
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST));
    m_commandList->ResolveSubresource(m_renderTargets[m_frameIndex].Get(), D3D12CalcSubresource(0, 0, 0, 1, 1), m_textureMSAA.Get(), D3D12CalcSubresource(0, 0, 0, 1, 1), DXGI_FORMAT_R8G8B8A8_UNORM);
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}

void Renderer::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
