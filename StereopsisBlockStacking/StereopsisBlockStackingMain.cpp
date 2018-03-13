#include "pch.h"
#include "StereopsisBlockStackingMain.h"
#include "Common\DirectXHelper.h"
#include "LPGL\lpgl.h"

#include <windows.graphics.directx.direct3d11.interop.h>
#include <Collection.h>

#include <DirectXCollision.h>


using namespace StereopsisBlockStacking;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;

// Loads and initializes application assets when the application is loaded.
StereopsisBlockStackingMain::StereopsisBlockStackingMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources)
{
    // Register to be notified if the device is lost or recreated.
    m_deviceResources->RegisterDeviceNotify(this);
}

void StereopsisBlockStackingMain::SetHolographicSpace(HolographicSpace^ holographicSpace)
{
    UnregisterHolographicEventHandlers();

    m_holographicSpace = holographicSpace;

	lpglInit(m_deviceResources);

	for (int i = 0; i < 5; ++i) {
		m_cubeRenderers.push_back(std::make_unique<SpinningCubeRenderer>());
	}

    m_spatialInputHandler = std::make_unique<SpatialInputHandler>();

    m_locator = SpatialLocator::GetDefault();

    m_locatabilityChangedToken =
        m_locator->LocatabilityChanged +=
            ref new Windows::Foundation::TypedEventHandler<SpatialLocator^, Object^>(
                std::bind(&StereopsisBlockStackingMain::OnLocatabilityChanged, this, _1, _2)
                );

    m_cameraAddedToken =
        m_holographicSpace->CameraAdded +=
            ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraAddedEventArgs^>(
                std::bind(&StereopsisBlockStackingMain::OnCameraAdded, this, _1, _2)
                );

    m_cameraRemovedToken =
        m_holographicSpace->CameraRemoved +=
            ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraRemovedEventArgs^>(
                std::bind(&StereopsisBlockStackingMain::OnCameraRemoved, this, _1, _2)
                );

    m_referenceFrame = m_locator->CreateStationaryFrameOfReferenceAtCurrentLocation();
}

void StereopsisBlockStackingMain::UnregisterHolographicEventHandlers()
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

StereopsisBlockStackingMain::~StereopsisBlockStackingMain()
{
    m_deviceResources->RegisterDeviceNotify(nullptr);

    UnregisterHolographicEventHandlers();
}

HolographicFrame^ StereopsisBlockStackingMain::Update()
{
    HolographicFrame^ holographicFrame = m_holographicSpace->CreateNextFrame();

    HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

    m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

    SpatialCoordinateSystem^ currentCoordinateSystem = m_referenceFrame->CoordinateSystem;

    SpatialInteractionSourceState^ pointerState = m_spatialInputHandler->CheckForInput();

	if (pointerState != nullptr) {
		auto pose = pointerState->TryGetPointerPose(currentCoordinateSystem);

		switch (m_spatialInputHandler->m_spatialInputState) {
		case eSISPressed:
			for (int i = 0; i < m_cubeRenderers.size(); ++i) {
				DirectX::BoundingBox bb = m_cubeRenderers[i]->GetBoundingBox();

				float distance = 0.0f;

				if (bb.Intersects(
					DirectX::XMLoadFloat3(&pose->Head->Position),
					DirectX::XMLoadFloat3(&pose->Head->ForwardDirection),
					distance)) {
					m_cubeRenderers[i]->isGrabbed = true;

					m_pickedObject = m_cubeRenderers[i].get();

					break;
				}
			}
			break;
		case eSISMoved:
		{
			if (m_pickedObject) {
				auto diff = m_pickedObject->GetPosition() - pose->Head->Position;
				float distance = 0.0f;

				DirectX::XMStoreFloat(&distance, DirectX::XMVector3Length(DirectX::XMLoadFloat3(&diff)));

				m_pickedObject->SetPosition(pose->Head->Position + pose->Head->ForwardDirection * distance);
			}
			break;
		}
		case eSISReleased:
			m_pickedObject = nullptr;
			break;
		}
	}

	m_timer.Tick([&]()
	{
		for (int i = 0; i < m_cubeRenderers.size(); ++i)
			m_cubeRenderers[i]->Update(m_timer);
	});

	return holographicFrame;
}

bool StereopsisBlockStackingMain::Render(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame)
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
				for (int i = 0; i < m_cubeRenderers.size(); ++i)
					m_cubeRenderers[i]->Render();
			}
			atLeastOneCameraRendered = true;
		}

		return atLeastOneCameraRendered;
	});
}

void StereopsisBlockStackingMain::SaveAppState()
{
}

void StereopsisBlockStackingMain::LoadAppState()
{
}

void StereopsisBlockStackingMain::OnDeviceLost()
{
	for (int i = 0; i < m_cubeRenderers.size(); ++i)
		m_cubeRenderers[i]->ReleaseDeviceDependentResources();
}

void StereopsisBlockStackingMain::OnDeviceRestored()
{
	for (int i = 0; i < m_cubeRenderers.size(); ++i)
		m_cubeRenderers[i]->CreateDeviceDependentResources();
}

void StereopsisBlockStackingMain::OnLocatabilityChanged(SpatialLocator^ sender, Object^ args)
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

void StereopsisBlockStackingMain::OnCameraAdded(
	HolographicSpace^ sender,
	HolographicSpaceCameraAddedEventArgs^ args
)
{
	Deferral^ deferral = args->GetDeferral();
	HolographicCamera^ holographicCamera = args->Camera;
	create_task([this, deferral, holographicCamera]()
	{
		m_deviceResources->AddHolographicCamera(holographicCamera);

		deferral->Complete();
	});
}

void StereopsisBlockStackingMain::OnCameraRemoved(
	HolographicSpace^ sender,
	HolographicSpaceCameraRemovedEventArgs^ args
)
{
	create_task([this]()
	{
	});

	m_deviceResources->RemoveHolographicCamera(args->Camera);
}
