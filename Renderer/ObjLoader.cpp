#include "stdafx.h"
#include "ObjLoader.h"

void ObjLoader::Load(const wstring fname, Vertex** vb, UINT& vbCount) {
    UINT ibSize;
    short* indexBuffer;

    vector<ObjFace> faces = parseOBJ(fname);
    objToBuffers(faces, vb, &indexBuffer, vbCount, ibSize);
}

Vertex ObjLoader::vertBundleToVert(ObjVertBundle bundle) {
    Vertex v;
    v.position.x = bundle.vertex.x;
    v.position.y = bundle.vertex.y;
    v.position.z = bundle.vertex.z;

    v.normal.x = bundle.normal.x;
    v.normal.y = bundle.normal.y;
    v.normal.z = bundle.normal.z;

    v.texCoord.x = bundle.texCoord.u;
    v.texCoord.y = bundle.texCoord.v;
    return v;
}

void ObjLoader::objToBuffers(vector<ObjFace> faces, Vertex** vb, short** ib, UINT& vbCount, UINT& ibCount) {
    vector<Vertex> vertices;

    for (int i = 0; i < faces.size(); ++i) {
        ObjFace face = faces[i];

        for (int j = 1; j < face.vertices.size() - 1; ++j) {
            vertices.push_back(vertBundleToVert(face.vertices[0]));
            vertices.push_back(vertBundleToVert(face.vertices[j]));
            vertices.push_back(vertBundleToVert(face.vertices[j + 1]));
        }
    }

    *vb = new Vertex[vertices.size()];
    *ib = new short[vertices.size()];

    for (short i = 0; i < vertices.size(); ++i) {
        (*vb)[i] = vertices[i];
        (*ib)[i] = i;
    }

    vbCount = vertices.size();
    ibCount = vertices.size();
}

vector<ObjFace> ObjLoader::parseOBJ(const wstring fname) {
    using namespace std;
    ifstream file(fname);
    string str;

    vector<ObjVert> vertices;
    vector<ObjTexCoord> texes;
    vector<ObjVertNorm> normals;
    vector<ObjVertBundle> bundles;
    vector<ObjFace> faces;

    while (getline(file, str)) {
        istringstream iss(str);
        string prefix;
        iss >> prefix;

        // OBJ supports W-component for vertices, 2/3 texcoord components, faces with more than 3 vertices, etc.
        // For now, only support the subset of OBJ that we're expecting, and blow up if we receive unexpected input
        if (prefix == "v") {
            ObjVert vert;
            iss >> vert.x;
            iss >> vert.y;
            iss >> vert.z;
            assert(iss.eof() && "ObjLoader: Received more vertex data than expected");
            vertices.push_back(vert);
        }
        else if (prefix == "vt") {
            ObjTexCoord tex;
            iss >> tex.u;
            iss >> tex.v;
            assert(iss.eof() && "ObjLoader: Received more tex coord data than expected");
            texes.push_back(tex);
        }
        else if (prefix == "vn") {
            ObjVertNorm normal;
            iss >> normal.x;
            iss >> normal.y;
            iss >> normal.z;
            assert(iss.eof() && "ObjLoader: Received more normal data than expected");
            normals.push_back(normal);
        }
        else if (prefix == "f") {
            ObjFace face;
            string vertBundleString;

            // Hard-coding that we expect only 3 vertices per face
            for (int i = 0; i < 3; ++i) {
                iss >> vertBundleString;

                stringstream vertBundleStream(vertBundleString);
                string segment;
                std::vector<string> seglist;

                while (getline(vertBundleStream, segment, '/')) {
                    seglist.push_back(segment);
                }

                int vertIdx = stoi(seglist[0]) - 1;
                int texIdx = stoi(seglist[1]) - 1;
                int normIdx = stoi(seglist[2]) - 1;

                face.vertices.push_back(
                    ObjVertBundle{
                        vertices[vertIdx],
                        texes[texIdx],
                        normals[normIdx]
                    }
                );
            }

            assert(iss.eof() && "ObjLoader: Received more face vertices than expected");
            faces.push_back(face);
        }
        else {
            continue;
        }
    }

    return faces;
}
