#include "Pipeline.h"

#include <cassert>
#include <iostream>

#include "ConstantBuffer.h"
#include "Texture.h"

void Pipeline::Initialize(ID3D12Device* device, VertexShader* vertexShader, PixelShader* pixelShader)
{
	VShader = vertexShader;
	PShader = pixelShader;
	RootSignature = new DynamicRootSignature;
    RootSignature->Initialize(device, VShader, PShader);

	uint32_t totalDescriptorCount = 0;
	for(auto [_, descTable] : RootSignature->Parameters.DescriptorTableIndexMap)
	{
		for(auto [name, index] : descTable.IndexMap)
		{
			HeapIndexMap[name] = totalDescriptorCount + index;
		}
		for(auto range : descTable.DescriptorRanges)
		{
			totalDescriptorCount += range.NumDescriptors;
		}
	}

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.NumDescriptors = totalDescriptorCount;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(device->CreateDescriptorHeap(&descHeapDesc,
											   IID_PPV_ARGS(&DescriptorHeap)));
	DescriptorHeap->SetName(L"Descriptor Heap For CBV + SRV");


	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	//psoDesc.InputLayout = {Vertex::Description, _countof(Vertex::Description)};
	psoDesc.InputLayout = VShader->InputLayoutDesc;
	psoDesc.pRootSignature = RootSignature->rootSignature;

	psoDesc.VS = VShader->GetShaderByteCode();
	psoDesc.PS = PShader->GetShaderByteCode();

	D3D12_RASTERIZER_DESC rasterDesc;
	rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterDesc.FrontCounterClockwise = FALSE;
	rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterDesc.DepthClipEnable = TRUE;
	rasterDesc.MultisampleEnable = FALSE;
	rasterDesc.AntialiasedLineEnable = FALSE;
	rasterDesc.ForcedSampleCount = 0;
	rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	//psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState = rasterDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	D3D12_BLEND_DESC blendDesc;
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
		FALSE,
		FALSE,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    //psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleMask = UINT_MAX;

	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	try
	{
		ThrowIfFailed(device->CreateGraphicsPipelineState(
			&psoDesc, IID_PPV_ARGS(&PipelineState)));
	}
	catch (com_exception e)
	{
		std::cout << "Failed to create Graphics Pipeline! " << e.what();
	}

}

void Pipeline::SetPipelineState(ID3D12CommandAllocator* commandAllocator, ID3D12GraphicsCommandList* commandList)
{
	ThrowIfFailed(commandList->Reset(commandAllocator, PipelineState));

	commandList->SetGraphicsRootSignature(RootSignature->rootSignature);

	ID3D12DescriptorHeap* pDescriptorHeaps[] = {DescriptorHeap};
	commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

	D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle(DescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	commandList->SetGraphicsRootDescriptorTable(1, descriptorHandle);

	for(auto [idx, address] :  ConstantBufferAddresses)
	{
		commandList->SetGraphicsRootConstantBufferView(idx, address);
	}
}

void Pipeline::BindTexture(ID3D12Device* device, std::string name, class Texture* texture)
{
	assert(texture);

	if(HeapIndexMap.count(name) <= 0)
	{
		std::cout << "cant find texture named in heap " << std::endl;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = texture->Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle(DescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srvHandle.ptr = srvHandle.ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * HeapIndexMap[name];

	device->CreateShaderResourceView(texture->Resource, &srvDesc, srvHandle);
}

void Pipeline::BindConstantBuffer(std::string name, ConstantBuffer* constantBuffer)
{
	if(RootSignature->Parameters.FreeParameterIndexMap.count(name) <= 0)
	{
		std::cout << "cant find constant buffer in root params" << std::endl;
	}

	auto index = RootSignature->Parameters.FreeParameterIndexMap[name];
	ConstantBufferAddresses[index] = constantBuffer->Resource->GetGPUVirtualAddress();
}
