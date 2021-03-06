#include "stdafx.h"
#pragma once

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texCoord;
};

class SceneObject
{
public:
    struct Constants {
        XMFLOAT4X4 model;
    };

    SceneObject();
    ~SceneObject();

    void UploadVertices(const ComPtr<ID3D12Device>& device);
    void UploadConstants(const ComPtr<ID3D12Device>& device);
    void LoadTexture(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12GraphicsCommandList>& commandList, std::wstring fname);

    void CreateDescriptorHeap(const ComPtr<ID3D12Device>& device);

    // Vertex-related state
    Vertex* m_vertices;
    UINT m_vertexCount;
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    // Constant-related state
    Constants m_constants;
    ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_pConstantBufferData;

    // Texture-related state
    ComPtr<ID3D12Resource> m_textureUploadHeap;
    ComPtr<ID3D12Resource> m_texture;
    bool m_hasTexture;

    // Descriptor heap for this object
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
};