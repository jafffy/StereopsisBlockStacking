#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"

#include <DirectXMath.h>

namespace StereopsisBlockStacking
{
	class SceneNode
	{
	public:
		SceneNode(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void Initialize();
		void Destroy();

		void Update(const DX::StepTimer& timer);
		void Render();

		DirectX::XMFLOAT3 position;

        Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11GeometryShader>    m_geometryShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_modelConstantBuffer;
		ModelConstantBuffer m_modelConstantBufferData;

		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		bool m_loadingComplete = false;
	};
}