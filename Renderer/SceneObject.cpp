#include "stdafx.h"
#include "SceneObject.h"
#include "ImageLoader.h"

SceneObject::SceneObject() : m_hasTexture(false) {};

SceneObject::~SceneObject() {};

void SceneObject::UploadVertices(const ComPtr<ID3D12Device>& device) {
    const UINT bufferSize = m_vertexCount * sizeof(Vertex);

    // Note: using upload heaps to transfer static data like vert buffers is not 
    // recommended. Every time the GPU needs it, the upload heap will be marshalled 
    // over. Please read up on Default Heap usage. An upload heap is used here for 
    // code simplicity and because there are very few verts to actually transfer.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)
    ));

    // Copy the triangle data to the vertex buffer.
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, m_vertices, bufferSize);
    m_vertexBuffer->Unmap(0, nullptr);

    // Initialize the Vertex Buffer View
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = bufferSize;
}

void SceneObject::UploadConstants(const ComPtr<ID3D12Device>& device) {
    CreateDescriptorHeap(device);
    // If Constant Buffer + CBV + CBV Heap haven't been initialized, do so
    if (!m_constantBuffer) {
        // Create the constant buffer.
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constantBuffer)));

        // Create a constant buffer view in our newly-create heap, pointing to our constant buffer.
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = (sizeof(Constants) + 255) & ~255;    // CB size is required to be 256-byte aligned.
        device->CreateConstantBufferView(&cbvDesc, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pConstantBufferData)));
    }

    // Copy constant data to constant buffer
    Constants constants;
    XMMATRIX model = XMMatrixTranspose(XMLoadFloat4x4(&m_constants.model));
    XMStoreFloat4x4(&constants.model, model);

    memcpy(m_pConstantBufferData, &constants, sizeof(constants));
}

void SceneObject::LoadTexture(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList, const std::wstring fname) {
    CreateDescriptorHeap(device);
    ImageLoader imageLoader(fname.c_str());
    MipMap mip0(imageLoader.getMipMap(0));
    m_hasTexture = true;

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = mip0.width;
    textureDesc.Height = mip0.height;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_texture)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1) * 10;

    // Create the GPU upload buffer.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_textureUploadHeap)));

    // Copy data to the intermediate upload heap and then schedule a copy 
    // from the upload heap to the Texture2D.
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = mip0.bytes;
    textureData.RowPitch = mip0.width * sizeof(MipMap::Pixel);
    textureData.SlicePitch = textureData.RowPitch * mip0.height;

    UpdateSubresources(commandList.Get(), m_texture.Get(), m_textureUploadHeap.Get(), 0, 0, 1, &textureData);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    // Create shader resource view in descriptor heap
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    srvHandle.Offset(1, descriptorSize);

    device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHandle);
}

void SceneObject::CreateDescriptorHeap(const ComPtr<ID3D12Device>& device) {
    if (!m_descriptorHeap) {
        // Describe and create a descriptor heap.
        // Flags indicate that this descriptor heap can be bound to the pipeline 
        // and that descriptors contained in it can be referenced by a root table.
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = 2;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));
    }
}