#include "pch.h"
#include "SpinningCubeRenderer.h"
#include "Common\DirectXHelper.h"

#include "tiny_obj_loader.h"

using namespace StereopsisBlockStackingPlayer;
using namespace Concurrency;
using namespace DirectX;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Input::Spatial;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
SpinningCubeRenderer::SpinningCubeRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
void SpinningCubeRenderer::PositionHologram(SpatialPointerPose^ pointerPose)
{
    if (pointerPose != nullptr)
    {
        // Get the gaze direction relative to the given coordinate system.
        const float3 headPosition    = pointerPose->Head->Position;
        const float3 headDirection   = pointerPose->Head->ForwardDirection;

        // The hologram is positioned two meters along the user's gaze direction.
        constexpr float distanceFromUser    = 2.0f; // meters
        const float3 gazeAtTwoMeters        = headPosition + (distanceFromUser * headDirection);

        // This will be used as the translation component of the hologram's
        // model transform.
        SetPosition(gazeAtTwoMeters);
    }
}

// Called once per frame. Rotates the cube, and calculates and sets the model matrix
// relative to the position transform indicated by hologramPositionTransform.
void SpinningCubeRenderer::Update(const DX::StepTimer& timer)
{
    const XMMATRIX modelTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));

    XMStoreFloat4x4(&m_modelConstantBufferData.model, XMMatrixTranspose(modelTranslation));

    if (!m_loadingComplete)
    {
        return;
    }

    const auto context = m_deviceResources->GetD3DDeviceContext();

    context->UpdateSubresource(
        m_modelConstantBuffer.Get(),
        0,
        nullptr,
        &m_modelConstantBufferData,
        0,
        0
        );
}

// Renders one frame using the vertex and pixel shaders.
// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
// a pass-through geometry shader is also used to set the render 
// target array index.
void SpinningCubeRenderer::Render()
{
    // Loading is asynchronous. Resources must be created before drawing can occur.
    if (!m_loadingComplete)
    {
        return;
    }

    const auto context = m_deviceResources->GetD3DDeviceContext();

    // Each vertex is one instance of the VertexPositionColor struct.
    const UINT stride = sizeof(VertexPositionColor);
    const UINT offset = 0;
    context->IASetVertexBuffers(
        0,
        1,
        m_vertexBuffer.GetAddressOf(),
        &stride,
        &offset
        );
    context->IASetIndexBuffer(
        m_indexBuffer.Get(),
        DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
        0
        );
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(m_inputLayout.Get());

    // Attach the vertex shader.
    context->VSSetShader(
        m_vertexShader.Get(),
        nullptr,
        0
        );
    // Apply the model constant buffer to the vertex shader.
    context->VSSetConstantBuffers(
        0,
        1,
        m_modelConstantBuffer.GetAddressOf()
        );

    if (!m_usingVprtShaders)
    {
        // On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
        // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
        // a pass-through geometry shader is used to set the render target 
        // array index.
        context->GSSetShader(
            m_geometryShader.Get(),
            nullptr,
            0
            );
    }

    // Attach the pixel shader.
    context->PSSetShader(
        m_pixelShader.Get(),
        nullptr,
        0
        );

    // Draw the objects.
    context->DrawIndexedInstanced(
        m_indexCount,   // Index count per instance.
        2,              // Instance count.
        0,              // Start index location.
        0,              // Base vertex location.
        0               // Start instance location.
        );
}

void SpinningCubeRenderer::CreateDeviceDependentResources()
{
    m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

    // On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
    // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
    // we can avoid using a pass-through geometry shader to set the render
    // target array index, thus avoiding any overhead that would be 
    // incurred by setting the geometry shader stage.
    std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///VprtVertexShader.cso" : L"ms-appx:///VertexShader.cso";

    // Load shaders asynchronously.
    task<std::vector<byte>> loadVSTask = DX::ReadDataAsync(vertexShaderFileName);
    task<std::vector<byte>> loadPSTask = DX::ReadDataAsync(L"ms-appx:///PixelShader.cso");

    task<std::vector<byte>> loadGSTask;
    if (!m_usingVprtShaders)
    {
        // Load the pass-through geometry shader.
        loadGSTask = DX::ReadDataAsync(L"ms-appx:///GeometryShader.cso");
    }

    // After the vertex shader file is loaded, create the shader and input layout.
    task<void> createVSTask = loadVSTask.then([this] (const std::vector<byte>& fileData)
    {
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateVertexShader(
                fileData.data(),
                fileData.size(),
                nullptr,
                &m_vertexShader
                )
            );

        constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 2> vertexDesc =
        {{
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        }};

        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateInputLayout(
                vertexDesc.data(),
                static_cast<UINT>(vertexDesc.size()),
                fileData.data(),
                static_cast<UINT>(fileData.size()),
                &m_inputLayout
                )
            );
    });

    // After the pixel shader file is loaded, create the shader and constant buffer.
    task<void> createPSTask = loadPSTask.then([this] (const std::vector<byte>& fileData)
    {
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreatePixelShader(
                fileData.data(),
                fileData.size(),
                nullptr,
                &m_pixelShader
                )
            );

        const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateBuffer(
                &constantBufferDesc,
                nullptr,
                &m_modelConstantBuffer
                )
            );
    });

    task<void> createGSTask;
    if (!m_usingVprtShaders)
    {
        // After the pass-through geometry shader file is loaded, create the shader.
        createGSTask = loadGSTask.then([this] (const std::vector<byte>& fileData)
        {
            DX::ThrowIfFailed(
                m_deviceResources->GetD3DDevice()->CreateGeometryShader(
                    fileData.data(),
                    fileData.size(),
                    nullptr,
                    &m_geometryShader
                    )
                );
        });
    }

    // Once all shaders are loaded, create the mesh.
    task<void> shaderTaskGroup = m_usingVprtShaders ? (createPSTask && createVSTask) : (createPSTask && createVSTask && createGSTask);
    task<void> createCubeTask  = shaderTaskGroup.then([this] ()
    {
        std::vector<VertexPositionColor> cubeVertices;
        std::vector<unsigned short> cubeIndices;

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

        D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
        vertexBufferData.pSysMem                = cubeVertices.data();
        vertexBufferData.SysMemPitch            = 0;
        vertexBufferData.SysMemSlicePitch       = 0;
        const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColor) * static_cast<UINT>(cubeVertices.size()), D3D11_BIND_VERTEX_BUFFER);
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateBuffer(
                &vertexBufferDesc,
                &vertexBufferData,
                &m_vertexBuffer
                )
            );

        m_indexCount = static_cast<unsigned int>(cubeIndices.size());

        D3D11_SUBRESOURCE_DATA indexBufferData  = {0};
        indexBufferData.pSysMem                 = cubeIndices.data();
        indexBufferData.SysMemPitch             = 0;
        indexBufferData.SysMemSlicePitch        = 0;
        CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * static_cast<UINT>(cubeIndices.size()), D3D11_BIND_INDEX_BUFFER);
        DX::ThrowIfFailed(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
                &indexBufferDesc,
                &indexBufferData,
                &m_indexBuffer
                )
            );
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
    m_usingVprtShaders = false;
    m_vertexShader.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_geometryShader.Reset();
    m_modelConstantBuffer.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
}
