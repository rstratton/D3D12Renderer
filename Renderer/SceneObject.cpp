#include "stdafx.h"
#include "SceneObject.h"

SceneObject::SceneObject() {};

SceneObject::SceneObject(Vertex * vertices, Constants constants) : 
    m_vertices(vertices),
    m_constants(constants)
{
}

SceneObject::~SceneObject()
{
    free(m_vertices);
}
