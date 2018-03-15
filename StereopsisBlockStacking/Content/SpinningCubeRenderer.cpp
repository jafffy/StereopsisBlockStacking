#include "pch.h"
#include "SpinningCubeRenderer.h"
#include "Common\DirectXHelper.h"

#include "LPGL\lpgl.h"

using namespace StereopsisBlockStacking;
using namespace Concurrency;
using namespace DirectX;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Input::Spatial;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
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

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);

	glLoadIdentity();
	glTranslatef(m_position.x, m_position.y, m_position.z);

	glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_SHORT, nullptr, 2);
}

void SpinningCubeRenderer::CreateDeviceDependentResources()
{
	task<void> createCubeTask = create_task([this]()
	{
		float width = 0.05f;
		static const std::array<VertexPositionColor, 8> cubeVertices =
		{ {
			{ XMFLOAT3(-width, -width, -width), XMFLOAT3(1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(-width, -width,  width), XMFLOAT3(1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(-width,  width, -width), XMFLOAT3(1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(-width,  width,  width), XMFLOAT3(1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(width, -width, -width), XMFLOAT3(1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(width, -width,  width), XMFLOAT3(1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(width,  width, -width), XMFLOAT3(1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(width,  width,  width), XMFLOAT3(1.0f, 1.0f, 1.0f) },
		} };

		DirectX::BoundingBox::CreateFromPoints(
			initialBoundingBox,
			cubeVertices.size(),
			reinterpret_cast<const XMFLOAT3*>(cubeVertices.data()),
			sizeof(XMFLOAT3) * 2);

		glGenBuffers(1, &vertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPositionColor) * cubeVertices.size(),
			cubeVertices.data(), GL_STATIC_DRAW);

		constexpr std::array<unsigned short, 36> cubeIndices =
		{ {
			2,1,0, // -x
			2,3,1,

			6,4,5, // +x
			6,5,7,

			0,1,5, // -y
			0,5,4,

			2,6,7, // +y
			2,7,3,

			0,4,6, // -z
			0,6,2,

			1,3,7, // +z
			1,7,5,
		} };

		m_indexCount = static_cast<unsigned int>(cubeIndices.size());

		glGenBuffers(1, &indexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			sizeof(unsigned short) * cubeIndices.size(),
			cubeIndices.data(), GL_STATIC_DRAW);
	});

	// Once the cube is loaded, the object is ready to be rendered.
	createCubeTask.then([this]()
	{
		m_loadingComplete = true;
	});
}

void SpinningCubeRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
}

DirectX::BoundingBox SpinningCubeRenderer::GetBoundingBox() const
{
	DirectX::BoundingBox outBB;

    XMMATRIX modelTransform = 
    XMMatrixMultiply(
    XMMatrixScalingFromVector(XMLoadFloat3(&m_scale)),
    XMMatrixTranslationFromVector(XMLoadFloat3(&m_position))
    );

	initialBoundingBox.Transform(
		outBB,
		modelTransform
	);

	return outBB;
}
