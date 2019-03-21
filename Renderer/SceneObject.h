#include "stdafx.h"
#pragma once

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class SceneObject
{
public:
    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT4 color;
    };

    struct Constants {
        XMMATRIX objToWorld;
    };

    SceneObject();
    ~SceneObject();

    void UploadVertices(const ComPtr<ID3D12Device>& device);

    // Vertex-related state
    Vertex* m_vertices;
    UINT m_vertexCount;
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    // Constant-related state
    Constants m_constants;
};