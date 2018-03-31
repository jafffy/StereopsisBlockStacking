#include "pch.h"
#include "StereopsisBlockStackingPlayerMain.h"
#include "Common\DirectXHelper.h"

#include <windows.graphics.directx.direct3d11.interop.h>
#include <Collection.h>


using namespace StereopsisBlockStackingPlayer;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;

StereopsisBlockStackingPlayerMain::StereopsisBlockStackingPlayerMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources)
{
    m_deviceResources->RegisterDeviceNotify(this);
}

void StereopsisBlockStackingPlayerMain::SetHolographicSpace(HolographicSpace^ holographicSpace)
{
    UnregisterHolographicEventHandlers();

    m_holographicSpace = holographicSpace;

    // Load records
    {
        char buf[4096];

        FILE* fp;
        fopen_s(&fp, ".\\Assets\\smallMovement.txt", "rt");

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
        m_meshRenderers.push_back(std::make_unique<SpinningCubeRenderer>(m_deviceResources));
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

    HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

    m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

    SpatialCoordinateSystem^ currentCoordinateSystem = m_referenceFrame->CoordinateSystem;

    m_timer.Tick([&] ()
    {
        for (auto& meshRenderer : m_meshRenderers) {
            meshRenderer->Update(m_timer);
        }
    });

    using namespace DirectX;

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
    //
    // TODO: Insert code here to save your app state.
    //       This method is called when the app is about to suspend.
    //
    //       For example, store information in the SpatialAnchorStore.
    //
}

void StereopsisBlockStackingPlayerMain::LoadAppState()
{
    //
    // TODO: Insert code here to load your app state.
    //       This method is called when the app resumes.
    //
    //       For example, load information from the SpatialAnchorStore.
    //
}

// Notifies classes that use Direct3D device resources that the device resources
// need to be released before this method returns.
void StereopsisBlockStackingPlayerMain::OnDeviceLost()
{
    for (auto& meshRenderer : m_meshRenderers) {
        meshRenderer->ReleaseDeviceDependentResources();
    }
}

// Notifies classes that use Direct3D device resources that the device resources
// may now be recreated.
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
        // Holograms cannot be rendered.
        {
            String^ message = L"Warning! Positional tracking is " +
                                        sender->Locatability.ToString() + L".\n";
            OutputDebugStringW(message->Data());
        }
        break;

    // In the following three cases, it is still possible to place holograms using a
    // SpatialLocatorAttachedFrameOfReference.
    case SpatialLocatability::PositionalTrackingActivating:
        // The system is preparing to use positional tracking.

    case SpatialLocatability::OrientationOnly:
        // Positional tracking has not been activated.

    case SpatialLocatability::PositionalTrackingInhibited:
        // Positional tracking is temporarily inhibited. User action may be required
        // in order to restore positional tracking.
        break;

    case SpatialLocatability::PositionalTrackingActive:
        // Positional tracking is active. World-locked content can be rendered.
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
        //
        // TODO: Allocate resources for the new camera and load any content specific to
        //       that camera. Note that the render target size (in pixels) is a property
        //       of the HolographicCamera object, and can be used to create off-screen
        //       render targets that match the resolution of the HolographicCamera.
        //

        // Create device-based resources for the holographic camera and add it to the list of
        // cameras used for updates and rendering. Notes:
        //   * Since this function may be called at any time, the AddHolographicCamera function
        //     waits until it can get a lock on the set of holographic camera resources before
        //     adding the new camera. At 60 frames per second this wait should not take long.
        //   * A subsequent Update will take the back buffer from the RenderingParameters of this
        //     camera's CameraPose and use it to create the ID3D11RenderTargetView for this camera.
        //     Content can then be rendered for the HolographicCamera.
        m_deviceResources->AddHolographicCamera(holographicCamera);

        // Holographic frame predictions will not include any information about this camera until
        // the deferral is completed.
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
        //
        // TODO: Asynchronously unload or deactivate content resources (not back buffer 
        //       resources) that are specific only to the camera that was removed.
        //
    });

    // Before letting this callback return, ensure that all references to the back buffer 
    // are released.
    // Since this function may be called at any time, the RemoveHolographicCamera function
    // waits until it can get a lock on the set of holographic camera resources before
    // deallocating resources for this camera. At 60 frames per second this wait should
    // not take long.
    m_deviceResources->RemoveHolographicCamera(args->Camera);
}
