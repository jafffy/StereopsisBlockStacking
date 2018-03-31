#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"
#include "LPGL\lpgl.h"

#include <DirectXCollision.h>

namespace StereopsisBlockStackingPlayer
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

        DirectX::BoundingBox GetBoundingBox() const;

        bool IsVisible = true;
        bool IsOutFocused = true;

    private:
        DirectX::BoundingBox initialBoundingBox;

        GLuint vertexBuffer;
        GLuint indexBuffer;

        uint32                                          m_indexCount = 0;

        GLuint outFocusVertexBuffer;
        GLuint outFocusIndexBuffer;

        uint32                                          m_indexCountOutFocused = 0;

        bool                                            m_loadingComplete = false;
        Windows::Foundation::Numerics::float3           m_position = { 0.f, 0.f, -2.f };
    };
}
