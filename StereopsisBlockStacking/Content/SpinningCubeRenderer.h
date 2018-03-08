#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"
#include "LPGL\lpgl.h"

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

    private:

		GLuint vertexBuffer;
		GLuint indexBuffer;

        // System resources for cube geometry.
        uint32                                          m_indexCount = 0;

        // Variables used with the rendering loop.
        bool                                            m_loadingComplete = false;
        Windows::Foundation::Numerics::float3           m_position = { 0.f, 0.f, -2.f };

    };
}
