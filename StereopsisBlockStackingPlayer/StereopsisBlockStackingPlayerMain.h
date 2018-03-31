#pragma once

//
// Comment out this preprocessor definition to disable all of the
// sample content.
//
// To remove the content after disabling it:
//     * Remove the unused code from your app's Main class.
//     * Delete the Content folder provided with this template.
//
#define DRAW_SAMPLE_CONTENT

#include "Common\DeviceResources.h"
#include "Common\StepTimer.h"

#ifdef DRAW_SAMPLE_CONTENT
#include "Content\SpinningCubeRenderer.h"
#include "Content\SpatialInputHandler.h"
#endif

#include <vector>

// Updates, renders, and presents holographic content using Direct3D.
namespace StereopsisBlockStackingPlayer
{
    class Record {
    public:
        long long timestamp;
        Windows::Foundation::Numerics::float3 headPosition;
        Windows::Foundation::Numerics::float3 headDirection;
        Windows::Foundation::Numerics::float3 mesh0;
        Windows::Foundation::Numerics::float3 mesh1;
        Windows::Foundation::Numerics::float3 mesh2;
        Windows::Foundation::Numerics::float3 mesh3;
        Windows::Foundation::Numerics::float3 mesh4;
    };

    class StereopsisBlockStackingPlayerMain : public DX::IDeviceNotify
    {
    public:
        StereopsisBlockStackingPlayerMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~StereopsisBlockStackingPlayerMain();

        void SetHolographicSpace(Windows::Graphics::Holographic::HolographicSpace^ holographicSpace);

        Windows::Graphics::Holographic::HolographicFrame^ Update();

        bool Render(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame);

        void SaveAppState();
        void LoadAppState();

        virtual void OnDeviceLost();
        virtual void OnDeviceRestored();

    private:
        void OnCameraAdded(
            Windows::Graphics::Holographic::HolographicSpace^ sender,
            Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs^ args);

        void OnCameraRemoved(
            Windows::Graphics::Holographic::HolographicSpace^ sender,
            Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs^ args);

        void OnLocatabilityChanged(
            Windows::Perception::Spatial::SpatialLocator^ sender,
            Platform::Object^ args);

        void UnregisterHolographicEventHandlers();

        std::vector<std::unique_ptr<SpinningCubeRenderer>>              m_meshRenderers;

        std::vector<Record> records;
        int currentRecordIndex = 0;

        std::shared_ptr<SpatialInputHandler>                            m_spatialInputHandler;

        std::shared_ptr<DX::DeviceResources>                            m_deviceResources;

        DX::StepTimer                                                   m_timer;

        Windows::Graphics::Holographic::HolographicSpace^               m_holographicSpace;

        Windows::Perception::Spatial::SpatialLocator^                   m_locator;

        Windows::Perception::Spatial::SpatialStationaryFrameOfReference^ m_referenceFrame;

        Windows::Foundation::EventRegistrationToken                     m_cameraAddedToken;
        Windows::Foundation::EventRegistrationToken                     m_cameraRemovedToken;
        Windows::Foundation::EventRegistrationToken                     m_locatabilityChangedToken;
    };
}
