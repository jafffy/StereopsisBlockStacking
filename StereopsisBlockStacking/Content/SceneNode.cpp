#include "pch.h"
#include "SceneNode.h"
#include "Common\DirectXHelper.h"

using namespace StereopsisBlockStacking;
using namespace Concurrency;
using namespace DirectX;

SceneNode::SceneNode(const std::shared_ptr<DX::DeviceResources>& deviceResources)
	: m_deviceResources(deviceResources)
{
	Initialize();
}

void SceneNode::Initialize()
{

}

void SceneNode::Update(const DX::StepTimer& timer)
{
}

void SceneNode::Render()
{
	if (!m_loadingComplete) {
		return;
	}

	const auto context = m_deviceResources->GetD3DDeviceContext();

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
		DXGI_FORMAT_R16_UINT,
		0
	);
}