#include "pch.h"
#include "SpinningCubeRenderer.h"
#include "Common\DirectXHelper.h"

#include "tiny_obj_loader.h"

using namespace StereopsisBlockStackingPlayer;
using namespace Concurrency;
using namespace DirectX;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Input::Spatial;

SpinningCubeRenderer::SpinningCubeRenderer()
{
    CreateDeviceDependentResources();
}

void SpinningCubeRenderer::Update(const DX::StepTimer& timer)
{
    if (!m_loadingComplete)
    {
        return;
    }
}

void SpinningCubeRenderer::Render()
{
    if (!m_loadingComplete)
    {
        return;
    }

    if (IsVisible) {
        if (IsOutFocused) {
            glBindBuffer(GL_ARRAY_BUFFER, outFocusVertexBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outFocusIndexBuffer);

            glLoadIdentity();
            glTranslatef(m_position.x, m_position.y, m_position.z);

            glDrawElementsInstanced(GL_TRIANGLES, m_indexCountOutFocused, GL_UNSIGNED_SHORT, nullptr, 2);
        }
        else {
            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);

            glLoadIdentity();
            glTranslatef(m_position.x, m_position.y, m_position.z);

            glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_SHORT, nullptr, 2);
        }
    }
}


struct BoundingBox3D {
    XMFLOAT3 Min, Max;

    BoundingBox3D()
        : Min(FLT_MAX, FLT_MAX, FLT_MAX),
        Max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}

    BoundingBox3D(const XMFLOAT3& Min, const XMFLOAT3& Max)
        : Min(Min), Max(Max) {}

    void AddPoint(const XMFLOAT3& p) {
        Min.x = p.x < Min.x ? p.x : Min.x;
        Min.y = p.y < Min.y ? p.y : Min.y;
        Min.z = p.z < Min.z ? p.z : Min.z;

        Max.x = p.x > Max.x ? p.x : Max.x;
        Max.y = p.y > Max.y ? p.y : Max.y;
        Max.z = p.z > Max.z ? p.z : Max.z;
    }

    bool IncludePoint(const XMFLOAT3& p) {
        return Min.x < p.x && Min.y < p.y && Min.z < p.z
            && Max.x > p.x && Max.y > p.y && Max.z > p.z;
    }
};

struct VoxelNode {
    std::vector<int> position; // size of position is depth. Root is empty
    BoundingBox3D* boundingBox = nullptr;

    VoxelNode* parent = nullptr;
    VoxelNode* children[8];
    bool isCompleteSubtree = false;
    bool isLeaf = false;

    VoxelNode() {
        memset(children, 0, sizeof(VoxelNode*) * 8);
    }
};

struct VoxelOctree {
    VoxelNode* rootNode = nullptr;
    std::vector<const BoundingBox3D*> boxGeometries;
};

void createSuboctree(VoxelNode** parent, const std::vector<XMFLOAT3*>& vertices, const BoundingBox3D& boundingBox, int depth) {
    assert(parent != nullptr && *parent != nullptr);
    XMVECTOR mid, Min, Max;
    Min = XMLoadFloat3(&boundingBox.Min);
    Max = XMLoadFloat3(&boundingBox.Max);
    mid = (Min + Max) * 0.5f;

    auto halfLength = Max - mid;
    auto halfLengthX = XMVectorSet(XMVectorGetX(halfLength), 0, 0, 0);
    auto halfLengthY = XMVectorSet(0, XMVectorGetY(halfLength), 0, 0);
    auto halfLengthZ = XMVectorSet(0, 0, XMVectorGetZ(halfLength), 0);

    BoundingBox3D childrensBoundingBox[8];

    XMStoreFloat3(&childrensBoundingBox[0].Min, Min + halfLengthZ);
    XMStoreFloat3(&childrensBoundingBox[0].Max, mid + halfLengthZ);

    XMStoreFloat3(&childrensBoundingBox[1].Min, Min - halfLengthY);
    XMStoreFloat3(&childrensBoundingBox[1].Max, Max - halfLengthY);

    XMStoreFloat3(&childrensBoundingBox[2].Min, Min + halfLengthX);
    XMStoreFloat3(&childrensBoundingBox[2].Max, mid + halfLengthX);

    XMStoreFloat3(&childrensBoundingBox[3].Min, Min);
    XMStoreFloat3(&childrensBoundingBox[3].Max, mid);

    XMStoreFloat3(&childrensBoundingBox[4].Min, mid - halfLengthX);
    XMStoreFloat3(&childrensBoundingBox[4].Max, Max - halfLengthX);

    XMStoreFloat3(&childrensBoundingBox[5].Min, mid);
    XMStoreFloat3(&childrensBoundingBox[5].Max, Max);

    XMStoreFloat3(&childrensBoundingBox[6].Min, mid - halfLengthZ);
    XMStoreFloat3(&childrensBoundingBox[6].Max, Max - halfLengthZ);

    XMStoreFloat3(&childrensBoundingBox[7].Min, Min + halfLengthY);
    XMStoreFloat3(&childrensBoundingBox[7].Max, mid + halfLengthY);

    for (int i = 0; i < 8; ++i) {
        std::vector<XMFLOAT3*> subvertices;

        for (auto* vertex : vertices) {
            if (childrensBoundingBox[i].IncludePoint(*vertex)) {
                subvertices.push_back(vertex);
            }
        }

        if (subvertices.size() > 0) {
            (*parent)->children[i] = new VoxelNode();
            (*parent)->children[i]->parent = *parent;
            (*parent)->children[i]->position = (*parent)->position;
            (*parent)->children[i]->position.push_back(i);

            BoundingBox3D* pChildBoundingBox = new BoundingBox3D();
            *pChildBoundingBox = childrensBoundingBox[i];
            (*parent)->children[i]->boundingBox = pChildBoundingBox;

            if (depth - 1 > 0) {
                createSuboctree(&(*parent)->children[i], subvertices, childrensBoundingBox[i], depth - 1);
            }
            else {
                (*parent)->children[i]->isLeaf = true;
            }
        }
    }

    int isFull = 0;

    for (int i = 0; i < 8; ++i) {
        if (parent && (*parent)->children[i]
            && ((*parent)->children[i]->isLeaf || (*parent)->children[i]->isCompleteSubtree)) {
            isFull++;
        }
    }

    if (isFull == 8) {
        (*parent)->isCompleteSubtree = true;
    }
}

void BuildOctreeGeometry(VoxelOctree* octree, VoxelNode* parent)
{
    if (!octree)
        return;

    if (parent->isCompleteSubtree) {
        octree->boxGeometries.push_back(parent->boundingBox);
    }
    else {
        for (int i = 0; i < 8; ++i) {
            auto* child = parent->children[i];

            if (child) {
                if (child->isLeaf) {
                    octree->boxGeometries.push_back(child->boundingBox);
                }
                else {
                    BuildOctreeGeometry(octree, parent->children[i]);
                }
            }
        }
    }
}

VoxelOctree* createOctree(const std::vector<XMFLOAT3*>& vertices, int depth) {
    VoxelOctree* voxelOctree = new VoxelOctree();
    voxelOctree->rootNode = new VoxelNode();

    BoundingBox3D *bb = new BoundingBox3D();
    for (auto* vertex : vertices) {
        bb->AddPoint(*vertex);
    }
    voxelOctree->rootNode->boundingBox = bb;

    createSuboctree(&voxelOctree->rootNode, vertices, *bb, depth);

    return voxelOctree;
}

void SpinningCubeRenderer::CreateDeviceDependentResources()
{
    task<void> createCubeTask  = create_task([this] ()
    {
        std::vector<VertexPositionColor> cubeVertices;
        std::vector<unsigned short> cubeIndices;

        std::vector<VertexPositionColor> cubeVerticesOutFocused;
        std::vector<unsigned short> cubeIndicesOutFocused;

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, ".\\Assets\\cylinder.obj", ".\\Assets\\", false);

        if (!err.empty())
        {
            OutputDebugStringA(err.c_str());
        }

        if (!ret)
        {
            OutputDebugStringA("Failed to load/parse .obj.\n");
        }

        int N = 1;
        for (unsigned int n = 0; n < N; ++n) {
            for (unsigned int i = 0; i < attrib.vertices.size() / 3; ++i)
            {
                XMFLOAT3 v;
                v.x = attrib.vertices[3 * i + 0] + n * 0.025f * 0.5f;
                v.y = attrib.vertices[3 * i + 1];
                v.z = attrib.vertices[3 * i + 2];

                cubeVertices.push_back(VertexPositionColor(v, XMFLOAT3(1, 1, 1)));
            }

            for (unsigned int si = 0; si < shapes.size(); ++si)
            {
                int baseIndex = n * attrib.vertices.size() / 3;

                for (unsigned int ii = 0; ii < shapes[si].mesh.indices.size() / 3; ++ii)
                {
                    unsigned int a = baseIndex + shapes[si].mesh.indices[3 * ii + 0].vertex_index;
                    unsigned int b = baseIndex + shapes[si].mesh.indices[3 * ii + 1].vertex_index;
                    unsigned int c = baseIndex + shapes[si].mesh.indices[3 * ii + 2].vertex_index;

                    // CW order
                    cubeIndices.push_back(a);
                    cubeIndices.push_back(c);
                    cubeIndices.push_back(b);
                }
            }
        }

        DirectX::BoundingBox::CreateFromPoints(
            initialBoundingBox,
            cubeVertices.size(),
            reinterpret_cast<const XMFLOAT3*>(cubeVertices.data()),
            sizeof(XMFLOAT3) * 2);

        glGenBuffers(1, &vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPositionColor) * cubeVertices.size(),
            cubeVertices.data(), GL_STATIC_DRAW);

        m_indexCount = static_cast<unsigned int>(cubeIndices.size());

        glGenBuffers(1, &indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            sizeof(unsigned short) * cubeIndices.size(),
            cubeIndices.data(), GL_STATIC_DRAW);

        std::vector<XMFLOAT3*> vertices;

        for (int i = 0; i < attrib.vertices.size() / 3; ++i) {
            vertices.push_back(reinterpret_cast<XMFLOAT3*>(attrib.vertices.data() + 3 * i));
        }

        auto* voxelOctree = createOctree(vertices, 3);

        BuildOctreeGeometry(voxelOctree, voxelOctree->rootNode);

        {
            unsigned short index = 0;

            for (const auto* leafNodeBox : voxelOctree->boxGeometries) {
                auto &Min = leafNodeBox->Min;
                auto &Max = leafNodeBox->Max;

                cubeVerticesOutFocused.insert(cubeVerticesOutFocused.end(),
                { VertexPositionColor(XMFLOAT3(Min.x, Min.y, Min.z), XMFLOAT3(1, 1, 1)),
                    VertexPositionColor(XMFLOAT3(Min.x, Min.y, Max.z), XMFLOAT3(1, 1, 1)),
                    VertexPositionColor(XMFLOAT3(Min.x, Max.y, Min.z), XMFLOAT3(1, 1, 1)),
                    VertexPositionColor(XMFLOAT3(Min.x, Max.y, Max.z), XMFLOAT3(1, 1, 1)),
                    VertexPositionColor(XMFLOAT3(Max.x, Min.y, Min.z), XMFLOAT3(1, 1, 1)),
                    VertexPositionColor(XMFLOAT3(Max.x, Min.y, Max.z), XMFLOAT3(1, 1, 1)),
                    VertexPositionColor(XMFLOAT3(Max.x, Max.y, Min.z), XMFLOAT3(1, 1, 1)),
                    VertexPositionColor(XMFLOAT3(Max.x, Max.y, Max.z), XMFLOAT3(1, 1, 1)) });

                cubeIndicesOutFocused.insert(cubeIndicesOutFocused.end(),
                {
                    unsigned short(index + 2),unsigned short(index + 0),unsigned short(index + 1), // -x
                    unsigned short(index + 2),unsigned short(index + 1),unsigned short(index + 3),
                    unsigned short(index + 6),unsigned short(index + 5),unsigned short(index + 4), // +x
                    unsigned short(index + 6),unsigned short(index + 7),unsigned short(index + 5),
                    unsigned short(index + 0),unsigned short(index + 5),unsigned short(index + 1), // -y
                    unsigned short(index + 0),unsigned short(index + 4),unsigned short(index + 5),
                    unsigned short(index + 2),unsigned short(index + 7),unsigned short(index + 6), // +y
                    unsigned short(index + 2),unsigned short(index + 3),unsigned short(index + 7),
                    unsigned short(index + 0),unsigned short(index + 6),unsigned short(index + 4), // -z
                    unsigned short(index + 0),unsigned short(index + 2),unsigned short(index + 6),
                    unsigned short(index + 1),unsigned short(index + 7),unsigned short(index + 3), // +z
                    unsigned short(index + 1),unsigned short(index + 5),unsigned short(index + 7),
                });

                index += 8;
            }
        }

        glGenBuffers(1, &outFocusVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, outFocusVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPositionColor) * cubeVerticesOutFocused.size(),
            cubeVerticesOutFocused.data(), GL_STATIC_DRAW);

        m_indexCountOutFocused = static_cast<unsigned int>(cubeIndicesOutFocused.size());

        glGenBuffers(1, &outFocusIndexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outFocusIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            sizeof(unsigned short) * cubeIndicesOutFocused.size(),
            cubeIndicesOutFocused.data(), GL_STATIC_DRAW);
    });

    // Once the cube is loaded, the object is ready to be rendered.
    createCubeTask.then([this] ()
    {
        m_loadingComplete = true;
    });
}

void SpinningCubeRenderer::ReleaseDeviceDependentResources()
{
    m_loadingComplete  = false;
}

DirectX::BoundingBox SpinningCubeRenderer::GetBoundingBox() const
{
    DirectX::BoundingBox outBB;

    XMMATRIX modelTransform = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));

    initialBoundingBox.Transform(
        outBB,
        modelTransform
    );

    return outBB;
}
