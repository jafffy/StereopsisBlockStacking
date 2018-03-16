#include "pch.h"
#include "lpgl.h"

#include "Content\ShaderStructures.h"
#include "Common\DirectXHelper.h"

#include <stack>
#include <unordered_map>

using namespace DirectX;
using namespace Concurrency;

static std::shared_ptr<DX::DeviceResources> gDeviceResources;

struct __lpglBuffer {
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
};

static struct lpglContext {
	std::stack<XMFLOAT4X4> transform;

	Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vertexShader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader>    m_geometryShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> modelConstantBuffer;

	bool                                            m_usingVprtShaders = false;

	ModelConstantBuffer modelConstantBufferData;

	GLuint idGen = 1;

	void Initialize();

	GLuint activeArrayBuffer = -1;
	GLuint activeElementArrayBuffer = -1;

	std::unordered_map<GLuint, __lpglBuffer*> buffers;
} gLpglContext;

void lpglContext::Initialize()
{
	const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
	lpglGetDeviceResources().GetD3DDevice()->CreateBuffer(
		&constantBufferDesc,
		nullptr,
		&modelConstantBuffer
	);

	m_usingVprtShaders = lpglGetDeviceResources().GetDeviceSupportsVprt();

	std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///VprtVertexShader.cso" : L"ms-appx:///VertexShader.cso";

	task<std::vector<byte>> loadVSTask = DX::ReadDataAsync(vertexShaderFileName);
	task<std::vector<byte>> loadPSTask = DX::ReadDataAsync(L"ms-appx:///PixelShader.cso");

	task<std::vector<byte>> loadGSTask;
	if (!m_usingVprtShaders)
	{
		loadGSTask = DX::ReadDataAsync(L"ms-appx:///GeometryShader.cso");
	}

	task<void> createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(
			lpglGetDeviceResources().GetD3DDevice()->CreateVertexShader(
				fileData.data(),
				fileData.size(),
				nullptr,
				&m_vertexShader
			)
		);

		constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 2> vertexDesc =
		{ {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		} };

		DX::ThrowIfFailed(
			lpglGetDeviceResources().GetD3DDevice()->CreateInputLayout(
				vertexDesc.data(),
				static_cast<UINT>(vertexDesc.size()),
				fileData.data(),
				static_cast<UINT>(fileData.size()),
				&m_inputLayout
			)
		);
	});

	task<void> createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(
			lpglGetDeviceResources().GetD3DDevice()->CreatePixelShader(
				fileData.data(),
				fileData.size(),
				nullptr,
				&m_pixelShader
			)
		);
	});

	task<void> createGSTask;
	if (!m_usingVprtShaders)
	{
		createGSTask = loadGSTask.then([this](const std::vector<byte>& fileData)
		{
			DX::ThrowIfFailed(
				lpglGetDeviceResources().GetD3DDevice()->CreateGeometryShader(
					fileData.data(),
					fileData.size(),
					nullptr,
					&m_geometryShader
				)
			);
		});
	}
}

void lpglInit(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	gDeviceResources = deviceResources;

	gLpglContext.Initialize();
}

DX::DeviceResources& lpglGetDeviceResources()
{
	return *gDeviceResources;
}

void __lpglDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei primcount)
{
	auto context = lpglGetDeviceResources().GetD3DDeviceContext();

    context->IASetInputLayout(gLpglContext.m_inputLayout.Get());

	context->VSSetShader(
		gLpglContext.m_vertexShader.Get(),
		nullptr,
		0
	);
	context->PSSetShader(
		gLpglContext.m_pixelShader.Get(),
		nullptr,
		0
	);

    if (!gLpglContext.m_usingVprtShaders)
    {
        // On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
        // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
        // a pass-through geometry shader is used to set the render target 
        // array index.
        context->GSSetShader(
            gLpglContext.m_geometryShader.Get(),
            nullptr,
            0
            );
    }

	XMStoreFloat4x4(
		&gLpglContext.modelConstantBufferData.model,
		XMMatrixTranspose(XMLoadFloat4x4(&gLpglContext.transform.top())));
	context->UpdateSubresource(
		gLpglContext.modelConstantBuffer.Get(),
		0,
		nullptr,
		&gLpglContext.modelConstantBufferData,
		0,
		0
	);
	context->VSSetConstantBuffers(
		0,
		1,
		gLpglContext.modelConstantBuffer.GetAddressOf()
	);

	const UINT stride = sizeof(VertexPositionColor);
	const UINT offset = 0;
	context->IASetVertexBuffers(
		0,
		1,
		gLpglContext.buffers[gLpglContext.activeArrayBuffer]->buffer.GetAddressOf(),
		&stride,
		&offset
	);

	DXGI_FORMAT indexBufferFormat;

	switch (type) {
	case GL_UNSIGNED_SHORT:
		indexBufferFormat = DXGI_FORMAT_R16_UINT;
	}

	context->IASetIndexBuffer(
		gLpglContext.buffers[gLpglContext.activeElementArrayBuffer]->buffer.Get(),
		indexBufferFormat,
		0
	);

	switch (mode) {
	case GL_TRIANGLES:
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		break;
	}

	context->DrawIndexedInstanced(
		count,
		primcount,
		0,
		0,
		0
	);
}

void __lpglLoadIdentity() {
	if (gLpglContext.transform.empty()) {
		XMFLOAT4X4 mat;
		XMStoreFloat4x4(&mat, XMMatrixIdentity());
		gLpglContext.transform.push(mat);
	}
	else {
		XMFLOAT4X4 mat;
		XMStoreFloat4x4(&mat, XMMatrixIdentity());
		gLpglContext.transform.top() = mat;
	}
}

void __lpglTranslatef(float x, float y, float z) {
	if (gLpglContext.transform.empty()) {
		XMFLOAT4X4 mat;
		XMStoreFloat4x4(&mat, XMMatrixTranslation(x, y, z));
		gLpglContext.transform.push(mat);
	}
	else {
		XMStoreFloat4x4(&gLpglContext.transform.top(),
			XMMatrixMultiply(
				XMLoadFloat4x4(&gLpglContext.transform.top()),
				XMMatrixTranslation(x, y, z)
			));
	}
}

void __lpglScalef(float x, float y, float z) {
	if (gLpglContext.transform.empty()) {
		XMFLOAT4X4 mat;
		XMStoreFloat4x4(&mat, XMMatrixScaling(x, y, z));
		gLpglContext.transform.push(mat);
	}
	else {
		XMStoreFloat4x4(&gLpglContext.transform.top(),
			XMMatrixMultiply(
				XMLoadFloat4x4(&gLpglContext.transform.top()),
				XMMatrixScaling(x, y, z)
			));
	}
}

void __lpglGenBuffers(GLsizei n, GLuint * buffers)
{
	for (int i = 0; i < n; ++i) {
		buffers[i] = gLpglContext.idGen++;
	}
}

void __lpglBindBuffer(GLenum target, GLuint buffer)
{
	switch (target) {
	case GL_ARRAY_BUFFER:
		gLpglContext.activeArrayBuffer = buffer;
		break;
	case GL_ELEMENT_ARRAY_BUFFER:
		gLpglContext.activeElementArrayBuffer = buffer;
		break;
	}

	if (gLpglContext.buffers.find(buffer) == gLpglContext.buffers.end()) {
		gLpglContext.buffers.insert_or_assign(buffer, new __lpglBuffer());
	}
}

void __lpglBufferData(GLenum target, GLsizei size, const GLvoid* data, GLenum usage)
{
	GLuint activeArrayBufferID = 0;
	D3D11_BIND_FLAG bindFlag;

	switch (target) {
	case GL_ARRAY_BUFFER:
		activeArrayBufferID = gLpglContext.activeArrayBuffer;
		bindFlag = D3D11_BIND_VERTEX_BUFFER;
		break;
	case GL_ELEMENT_ARRAY_BUFFER:
		activeArrayBufferID = gLpglContext.activeElementArrayBuffer;
		bindFlag = D3D11_BIND_INDEX_BUFFER;
		break;
	}

	auto bufIter = gLpglContext.buffers.find(activeArrayBufferID);

	if (bufIter == gLpglContext.buffers.end())
		return;

	D3D11_SUBRESOURCE_DATA bufferData = { 0 };
	bufferData.pSysMem = data;
	bufferData.SysMemPitch = 0;
	bufferData.SysMemSlicePitch = 0;

	const CD3D11_BUFFER_DESC bufferDesc(
		static_cast<UINT>(size),
		bindFlag);

	DX::ThrowIfFailed(
		lpglGetDeviceResources().GetD3DDevice()->CreateBuffer(
			&bufferDesc,
			&bufferData,
			&bufIter->second->buffer
		)
	);
}
