#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"
#include "LPGL\lpgl.h"

#include <DirectXCollision.h>

namespace StereopsisBlockStacking
{
    // This sample renderer instantiates a basic rendering pipeline.
    class SpinningCubeRenderer
    {
    public:
        SpinningCubeRenderer();
        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Update(const DX::StepTimer& timer);
        void Render();

        // Property accessors.
        void SetPosition(Windows::Foundation::Numerics::float3 pos) { m_position = pos;  }
        Windows::Foundation::Numerics::float3 GetPosition()         { return m_position; }

        void SetScale(Windows::Foundation::Numerics::float3 scale) { m_scale = scale; }
        Windows::Foundation::Numerics::float3 GetScale() const { return m_scale; }

		DirectX::BoundingBox GetBoundingBox() const;

		bool isGrabbed = false;

    private:
		DirectX::BoundingBox initialBoundingBox;

		GLuint vertexBuffer;
		GLuint indexBuffer;

        // System resources for cube geometry.
        uint32                                          m_indexCount = 0;

        // Variables used with the rendering loop.
        bool                                            m_loadingComplete = false;
        Windows::Foundation::Numerics::float3           m_position = { 0.f, 0.f, -2.f };
        Windows::Foundation::Numerics::float3           m_scale = { 1.0f, 1.0f, 1.0f };

    };
}
