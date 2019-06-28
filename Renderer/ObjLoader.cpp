#include "stdafx.h"
#include "ObjLoader.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

void ObjLoader::Load(const std::string fname, Vertex** vb, UINT& vbSize) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fname.c_str(), "", true)) {
        OutputDebugStringA("Failed to parse .obj file");
        exit(1);
    }

    if (!err.empty()) {
        OutputDebugStringA(err.c_str());
        exit(1);
    }

    if (!warn.empty()) {
        OutputDebugStringA(warn.c_str());
    }

    // Pretty sure we can know up front how many vertices we need to allocate space for;
    // could just build the raw array here instead of using a vector
    std::vector<Vertex> vertices;

    // Iterate over shapes
    for (size_t s = 0; s < shapes.size(); ++s) {
        size_t index_offset = 0;

        // Iterate over faces
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); ++f) {
            int fv = shapes[s].mesh.num_face_vertices[f];

            // Iterate over face vertices
            for (size_t v = 0; v < fv; ++v) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                Vertex vert;
                vert.position.x = attrib.vertices[3*idx.vertex_index+0];
                vert.position.y = attrib.vertices[3*idx.vertex_index+1];
                vert.position.z = attrib.vertices[3*idx.vertex_index+2];
                //vert.normal.x = attrib.vertices[3*idx.normal_index+0];
                //vert.normal.y = attrib.vertices[3*idx.normal_index+1];
                //vert.normal.z = attrib.vertices[3*idx.normal_index+2];
                vert.texCoord.x = attrib.vertices[3*idx.texcoord_index+0];
                vert.texCoord.y = attrib.vertices[3*idx.texcoord_index+1];

                vertices.push_back(vert);
            }

            XMVECTOR v1 = XMLoadFloat3(&vertices[vertices.size() - 3].position);
            XMVECTOR v2 = XMLoadFloat3(&vertices[vertices.size() - 2].position);
            XMVECTOR v3 = XMLoadFloat3(&vertices[vertices.size() - 1].position);

            XMVECTOR side1 = v1 - v2;
            XMVECTOR side2 = v3 - v2;
            XMVECTOR normal = XMVector3Cross(side2, side1);

            XMStoreFloat3(&vertices[vertices.size() - 3].normal, normal);
            XMStoreFloat3(&vertices[vertices.size() - 2].normal, normal);
            XMStoreFloat3(&vertices[vertices.size() - 1].normal, normal);

            index_offset += fv;
        }
    }

    *vb = new Vertex[vertices.size()];

    for (int i = 0; i < vertices.size(); ++i) {
        (*vb)[i] = vertices[i];
    }

    vbSize = vertices.size();
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
