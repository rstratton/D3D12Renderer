#pragma once
#include "SceneObject.h"

using namespace std;

struct ObjVert {
    float x, y, z;
};

struct ObjTexCoord {
    float u, v;
};

struct ObjVertNorm {
    float x, y, z;
};

struct ObjVertBundle {
    ObjVert vertex;
    ObjTexCoord texCoord;
    ObjVertNorm normal;
};

struct ObjFace {
    vector<ObjVertBundle> vertices;
};

class ObjLoader
{
public:
    static void Load(const wstring fname, Vertex** vb, UINT& vbSize);
private:
    static Vertex vertBundleToVert(ObjVertBundle bundle);
    static void objToBuffers(vector<ObjFace> faces, Vertex** vb, short** ib, UINT& vbSize, UINT& ibSize);
    static vector<ObjFace> parseOBJ(const wstring fname);
    ObjLoader();
};