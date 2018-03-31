#include "pch.h"
#include "StereopsisBlockStackingPlayerMain.h"
#include "Common\DirectXHelper.h"
#include "LPGL\lpgl.h"
#include "Common\FramerateController.h"

#include <windows.graphics.directx.direct3d11.interop.h>
#include <Collection.h>
#include <string>


using namespace StereopsisBlockStackingPlayer;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;

using namespace DirectX;

#define LOW
#ifdef LOW
static struct {
    float level1 = 0.7f;
    float level2 = 0.5f;
} threshold;
#endif
#ifdef HIGH
static struct {
    float level1 = 0.4f;
    float level2 = 0.2f;
} threshold;
#endif
#ifdef ORIGIN
static struct {
    float level1 = 0.0f;
    float level2 = 0.0f;
} threshold;
#endif

struct BoundingBox2D
{
    XMFLOAT2 Min;
    XMFLOAT2 Max;

    BoundingBox2D()
        : Min(FLT_MAX, FLT_MAX),
        Max(-FLT_MAX, -FLT_MAX)
    {}

    BoundingBox2D(const XMFLOAT2& Min, const XMFLOAT2& Max)
        : Min(Min), Max(Max) {}

    float Width() const { return Max.x - Min.x; }
    float Height() const { return Max.y - Min.y; }

    void AddPoint(float x, float y)
    {
        Min.x = x < Min.x ? x : Min.x;
        Min.y = y < Min.y ? y : Min.y;
        Max.x = x > Max.x ? x : Max.x;
        Max.y = y > Max.y ? y : Max.y;
    }

    bool IncludePoint(const XMFLOAT2& v) const
    {
        return Min.x < v.x && Min.y < v.y && Max.x > v.x && Max.y > v.y;
    }

    bool Intersect(const BoundingBox2D& bb) const
    {
        return IncludePoint(bb.Min) || IncludePoint(bb.Max);
    }
};

struct BoundingBox3D
{
    XMFLOAT3 Min;
    XMFLOAT3 Max;

    XMFLOAT3 vertices[8];

    BoundingBox3D()
        : Min(FLT_MAX, FLT_MAX, FLT_MAX),
        Max(-FLT_MAX, -FLT_MAX, -FLT_MAX)
    {}

    void AddPoint(const XMFLOAT3 &p)
    {
        float* m = &Min.x;
        float* M = &Max.x;
        const float* pPoint = &p.x;

        for (int i = 0; i < 3; ++i) {
            if (m[i] > pPoint[i]) {
                m[i] = pPoint[i];
            }
            if (M[i] < pPoint[i]) {
                M[i] = pPoint[i];
            }
        }
    }

    void BuildGeometry()
    {
        vertices[0] = Min;
        vertices[1] = Max;
        vertices[2] = XMFLOAT3(Min.x, Max.y, Max.z);
        vertices[3] = XMFLOAT3(Min.x, Max.y, Min.z);
        vertices[4] = XMFLOAT3(Max.x, Max.y, Min.z);
        vertices[5] = XMFLOAT3(Min.x, Min.y, Max.z);
        vertices[6] = XMFLOAT3(Max.x, Min.y, Max.z);
        vertices[7] = XMFLOAT3(Max.x, Min.y, Min.z);
    }
};

struct QuadTree
{
    struct Node
    {
        Node* parent = nullptr;
        Node* children[4] = { nullptr, nullptr, nullptr, nullptr };

        bool isFull = false;
        int depth;
    };

    Node* rootNode = nullptr;

    static QuadTree* Create(const std::vector<const BoundingBox2D*>& geometries, const int kMaxDepth);

    ~QuadTree()
    {
        DeleteSubtree(rootNode);

        if (rootNode) {
            delete rootNode;
            rootNode = nullptr;
        }
    }

    void DeleteSubtree(Node* node)
    {
        if (!node)
            return;

        for (auto* child : node->children) {
            DeleteSubtree(child);

            if (child) {
                delete child;
                child = nullptr;
            }
        }
    }
};

QuadTree* QuadTree::Create(const std::vector<const BoundingBox2D*>& geometries, const int kMaxDepth)
{
    QuadTree* res = new QuadTree();

    std::function<void(const BoundingBox2D&, const std::vector<const BoundingBox2D*>&, int, QuadTree::Node*)> addDepth
        = [&](const BoundingBox2D& area, const std::vector<const BoundingBox2D*>& geometries, int depth, QuadTree::Node* parent) {
        auto& Min = area.Min;
        auto& Max = area.Max;
        float halfWidth = area.Width() * 0.5f;
        float halfHeight = area.Height() * 0.5f;
        std::vector<BoundingBox2D> subareas;
        subareas.push_back(BoundingBox2D(XMFLOAT2(Min.x, Min.y), XMFLOAT2(Min.x + halfWidth, Min.y + halfHeight)));
        subareas.push_back(BoundingBox2D(XMFLOAT2(Min.x + halfWidth, Min.y), XMFLOAT2(Max.x, Min.y + halfHeight)));
        subareas.push_back(BoundingBox2D(XMFLOAT2(Min.x, Min.y + halfHeight), XMFLOAT2(Min.x + halfWidth, Max.y)));
        subareas.push_back(BoundingBox2D(XMFLOAT2(Min.x + halfWidth, Min.y + halfHeight), XMFLOAT2(Max.x, Max.y)));

        for (int i = 0; i < subareas.size(); ++i) {
            auto& subarea = subareas[i];

            for (auto* geometry : geometries) {
                if (subarea.Intersect(*geometry)) {
                    QuadTree::Node* pNode = new QuadTree::Node();
                    parent->children[i] = pNode;
                    pNode->parent = parent;
                    pNode->isFull = false;
                    pNode->depth = depth;

                    if (depth + 1 < kMaxDepth) {
                        addDepth(subarea, geometries, depth + 1, pNode);
                    }

                    break;
                }
            }
        }

        parent->isFull = parent->children[0] && parent->children[1] && parent->children[2] && parent->children[3];
    };

    BoundingBox2D viewArea(XMFLOAT2(-1, -1), XMFLOAT2(1, 1));
    QuadTree::Node* pRootNode = new QuadTree::Node();
    pRootNode->parent = nullptr;
    pRootNode->isFull = true;
    pRootNode->depth = 0;

    res->rootNode = pRootNode;

    addDepth(viewArea, geometries, 1, res->rootNode);

    return res;
}

static QuadTree* lastQuadTree = nullptr;

float GetDynamicScoreBasedOnQuadtree(const QuadTree::Node* prev, const QuadTree::Node* current)
{
    float sum = 0.0f;

    for (int i = 0; i < 4; ++i) {
        bool prevExist = prev->children[i];
        bool currentExist = current->children[i];

        if (prevExist && currentExist) {
            sum += GetDynamicScoreBasedOnQuadtree(prev->children[i], current->children[i]);
        }
        else if (prevExist != currentExist) {
            assert(prev->depth == current->depth);

            if (prevExist) {
                sum += 1.0f / (4 * prev->children[i]->depth);
            }
            else if (currentExist) {
                sum += 1.0f / (4 * current->children[i]->depth);
            }
        }
    }

    return sum;
}


StereopsisBlockStackingPlayerMain::StereopsisBlockStackingPlayerMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources)
{
    m_deviceResources->RegisterDeviceNotify(this);
}

void StereopsisBlockStackingPlayerMain::SetHolographicSpace(HolographicSpace^ holographicSpace)
{
    UnregisterHolographicEventHandlers();

    m_holographicSpace = holographicSpace;

    lpglInit(m_deviceResources);

    // Load records
    {
        char buf[4096];

        FILE* fp;
        fopen_s(&fp, ".\\Assets\\bigMovement.txt", "rt");

        while (NULL != fgets(buf, 4096, fp)) {
            Record record;

            sscanf_s(buf, "%llu,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
                &record.timestamp, &record.headPosition.x, &record.headPosition.y, &record.headPosition.z,
                &record.headDirection.x, &record.headDirection.y, &record.headDirection.z,
                &record.mesh0.x, &record.mesh0.y, &record.mesh0.z,
                &record.mesh1.x, &record.mesh1.y, &record.mesh1.z,
                &record.mesh2.x, &record.mesh2.y, &record.mesh2.z,
                &record.mesh3.x, &record.mesh3.y, &record.mesh3.z,
                &record.mesh4.x, &record.mesh4.y, &record.mesh4.z
            );

            records.push_back(record);
        }

        fclose(fp);
    }

    int N = 5;

    for (int i = 0; i < N; ++i) {
        m_meshRenderers.push_back(std::make_unique<SpinningCubeRenderer>());
    }

    m_spatialInputHandler = std::make_unique<SpatialInputHandler>();

    m_locator = SpatialLocator::GetDefault();

    m_locatabilityChangedToken =
        m_locator->LocatabilityChanged +=
            ref new Windows::Foundation::TypedEventHandler<SpatialLocator^, Object^>(
                std::bind(&StereopsisBlockStackingPlayerMain::OnLocatabilityChanged, this, _1, _2)
                );

    m_cameraAddedToken =
        m_holographicSpace->CameraAdded +=
            ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraAddedEventArgs^>(
                std::bind(&StereopsisBlockStackingPlayerMain::OnCameraAdded, this, _1, _2)
                );

    m_cameraRemovedToken =
        m_holographicSpace->CameraRemoved +=
            ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraRemovedEventArgs^>(
                std::bind(&StereopsisBlockStackingPlayerMain::OnCameraRemoved, this, _1, _2)
                );

    m_referenceFrame = m_locator->CreateStationaryFrameOfReferenceAtCurrentLocation();
}

void StereopsisBlockStackingPlayerMain::UnregisterHolographicEventHandlers()
{
    if (m_holographicSpace != nullptr)
    {
        if (m_cameraAddedToken.Value != 0)
        {
            m_holographicSpace->CameraAdded -= m_cameraAddedToken;
            m_cameraAddedToken.Value = 0;
        }

        if (m_cameraRemovedToken.Value != 0)
        {
            m_holographicSpace->CameraRemoved -= m_cameraRemovedToken;
            m_cameraRemovedToken.Value = 0;
        }
    }

    if (m_locator != nullptr)
    {
        m_locator->LocatabilityChanged -= m_locatabilityChangedToken;
    }
}

StereopsisBlockStackingPlayerMain::~StereopsisBlockStackingPlayerMain()
{
    // Deregister device notification.
    m_deviceResources->RegisterDeviceNotify(nullptr);

    UnregisterHolographicEventHandlers();
}

// Updates the application state once per frame.
HolographicFrame^ StereopsisBlockStackingPlayerMain::Update()
{
    HolographicFrame^ holographicFrame = m_holographicSpace->CreateNextFrame();
    holographicFrame->UpdateCurrentPrediction();

    HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

    m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

    SpatialCoordinateSystem^ currentCoordinateSystem = m_referenceFrame->CoordinateSystem;

    m_timer.Tick([&] ()
    {
        for (auto& meshRenderer : m_meshRenderers) {
            meshRenderer->Update(m_timer);
        }
    });

    for (auto cameraPose : prediction->CameraPoses) {
        auto coordinateSystem = m_referenceFrame->CoordinateSystem;

        Platform::IBox<HolographicStereoTransform>^ viewTransformContainer = cameraPose->TryGetViewTransform(coordinateSystem);
        HolographicStereoTransform viewCoordinateSystemTransform = viewTransformContainer->Value;
        auto viewMatrix = DirectX::XMLoadFloat4x4(&viewCoordinateSystemTransform.Left);

        HolographicStereoTransform cameraProjectionTransform = cameraPose->ProjectionTransform;
        auto projectionMatrix = DirectX::XMLoadFloat4x4(&cameraProjectionTransform.Left);

        auto VP = XMMatrixMultiply(viewMatrix, projectionMatrix);

        std::vector<const BoundingBox2D*> bbs;

        for (int i = 0; i < m_meshRenderers.size(); ++i) {
            BoundingBox bb = m_meshRenderers[i]->GetBoundingBox();
            XMFLOAT3 corners[8];
            auto* boundingBox2D = new BoundingBox2D();

            bb.GetCorners(corners);

            for (int vi = 0; vi < 8; ++vi) {
                XMFLOAT4 result;
                XMStoreFloat4(&result,
                    XMVector3TransformCoord(XMLoadFloat3(&corners[i]), VP));
                boundingBox2D->AddPoint(result.x, result.y);
            }

            bbs.push_back(boundingBox2D);
        }

        auto* quadTree = QuadTree::Create(bbs, 16);

        if (lastQuadTree) {
            float dynamicScore = GetDynamicScoreBasedOnQuadtree(lastQuadTree->rootNode, quadTree->rootNode);

            auto* framerateController = FramerateController::get();

            if (framerateController->GetFPS() > 60 - 1) {
                if (dynamicScore < threshold.level1) {
                    framerateController->SetFramerate(30);
                }
            }
            else if (framerateController->GetFPS() > 30 - 1) {
                if (dynamicScore > threshold.level1) {
                    framerateController->SetFramerate(60);
                }
                else if (dynamicScore < threshold.level2) {
                    framerateController->SetFramerate(15);
                }
            }
            else if (framerateController->GetFPS() > 15 - 1) {
                if (dynamicScore > threshold.level2) {
                    framerateController->SetFramerate(30);
                }
            }

            if (lastQuadTree) {
                delete lastQuadTree;
                lastQuadTree = nullptr;
            }
        }

        for (int i = 0; i < bbs.size(); ++i) {
            if (bbs[i]) {
                delete bbs[i];
                bbs[i] = nullptr;
            }
        }

        lastQuadTree = quadTree;

        break;
    }

    // Play back recorded data
    XMVECTOR headPosition = XMLoadFloat3(&records[currentRecordIndex].headPosition),
    headDirection = XMLoadFloat3(&records[currentRecordIndex].headDirection),
    upVector = XMVectorSet(0, 1, 0, 1),
    mesh0 = XMLoadFloat3(&records[currentRecordIndex].mesh0),
    mesh1 = XMLoadFloat3(&records[currentRecordIndex].mesh1),
    mesh2 = XMLoadFloat3(&records[currentRecordIndex].mesh2),
    mesh3 = XMLoadFloat3(&records[currentRecordIndex].mesh3),
    mesh4 = XMLoadFloat3(&records[currentRecordIndex].mesh4);

    XMMATRIX invCameraMatrix = XMMatrixLookAtRH(headPosition, headPosition + headDirection, upVector);

    Windows::Foundation::Numerics::float3 vmesh0, vmesh1, vmesh2, vmesh3, vmesh4;

    XMStoreFloat3(&vmesh0, XMVector3Transform(mesh0, invCameraMatrix));
    XMStoreFloat3(&vmesh1, XMVector3Transform(mesh1, invCameraMatrix));
    XMStoreFloat3(&vmesh2, XMVector3Transform(mesh2, invCameraMatrix));
    XMStoreFloat3(&vmesh3, XMVector3Transform(mesh3, invCameraMatrix));
    XMStoreFloat3(&vmesh4, XMVector3Transform(mesh4, invCameraMatrix));

    m_meshRenderers[0]->SetPosition(vmesh0);
    m_meshRenderers[1]->SetPosition(vmesh1);
    m_meshRenderers[2]->SetPosition(vmesh2);
    m_meshRenderers[3]->SetPosition(vmesh3);
    m_meshRenderers[4]->SetPosition(vmesh4);

    if (currentRecordIndex + 1 < records.size())
        currentRecordIndex++;
    else
        currentRecordIndex = 0;

    return holographicFrame;
}

bool StereopsisBlockStackingPlayerMain::Render(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame)
{
    if (m_timer.GetFrameCount() == 0)
    {
        return false;
    }

    return m_deviceResources->UseHolographicCameraResources<bool>(
        [this, holographicFrame](std::map<UINT32, std::unique_ptr<DX::CameraResources>>& cameraResourceMap)
    {
        holographicFrame->UpdateCurrentPrediction();
        HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

        bool atLeastOneCameraRendered = false;
        for (auto cameraPose : prediction->CameraPoses)
        {
            DX::CameraResources* pCameraResources = cameraResourceMap[cameraPose->HolographicCamera->Id].get();

            const auto context = m_deviceResources->GetD3DDeviceContext();
            const auto depthStencilView = pCameraResources->GetDepthStencilView();

            ID3D11RenderTargetView *const targets[1] = { pCameraResources->GetBackBufferRenderTargetView() };
            context->OMSetRenderTargets(1, targets, depthStencilView);

            context->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
            context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

            pCameraResources->UpdateViewProjectionBuffer(m_deviceResources, cameraPose, m_referenceFrame->CoordinateSystem);

            bool cameraActive = pCameraResources->AttachViewProjectionBuffer(m_deviceResources);

            if (cameraActive)
            {
                for (auto& meshRenderer : m_meshRenderers) {
                    meshRenderer->Render();
                }
            }
            atLeastOneCameraRendered = true;
        }

        return atLeastOneCameraRendered;
    });
}

void StereopsisBlockStackingPlayerMain::SaveAppState()
{
}

void StereopsisBlockStackingPlayerMain::LoadAppState()
{
}

void StereopsisBlockStackingPlayerMain::OnDeviceLost()
{
    for (auto& meshRenderer : m_meshRenderers) {
        meshRenderer->ReleaseDeviceDependentResources();
    }
}

void StereopsisBlockStackingPlayerMain::OnDeviceRestored()
{
    for (auto& meshRenderer : m_meshRenderers) {
        meshRenderer->CreateDeviceDependentResources();
    }
}

void StereopsisBlockStackingPlayerMain::OnLocatabilityChanged(SpatialLocator^ sender, Object^ args)
{
    switch (sender->Locatability)
    {
    case SpatialLocatability::Unavailable:
        {
            String^ message = L"Warning! Positional tracking is " +
                                        sender->Locatability.ToString() + L".\n";
            OutputDebugStringW(message->Data());
        }
        break;

    case SpatialLocatability::PositionalTrackingActivating:

    case SpatialLocatability::OrientationOnly:

    case SpatialLocatability::PositionalTrackingInhibited:
        break;

    case SpatialLocatability::PositionalTrackingActive:
        break;
    }
}

void StereopsisBlockStackingPlayerMain::OnCameraAdded(
    HolographicSpace^ sender,
    HolographicSpaceCameraAddedEventArgs^ args
    )
{
    Deferral^ deferral = args->GetDeferral();
    HolographicCamera^ holographicCamera = args->Camera;
    create_task([this, deferral, holographicCamera] ()
    {
        m_deviceResources->AddHolographicCamera(holographicCamera);

        deferral->Complete();
    });
}

void StereopsisBlockStackingPlayerMain::OnCameraRemoved(
    HolographicSpace^ sender,
    HolographicSpaceCameraRemovedEventArgs^ args
    )
{
    create_task([this]()
    {
    });

    m_deviceResources->RemoveHolographicCamera(args->Camera);
}
