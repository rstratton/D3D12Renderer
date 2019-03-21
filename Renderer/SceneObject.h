#include "stdafx.h"
#pragma once

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
    SceneObject(Vertex* vertices, Constants constants);
    ~SceneObject();

    Vertex* m_vertices;
    UINT m_vertexCount;
    Constants m_constants;
};

