#pragma once
#include "DynamicRootSignature.h"
#include "Shader.h"

class Pipeline
{
public:
	void Initialize(ID3D12Device* device, VertexShader* vertexShader, PixelShader* pixelShader);

	void SetPipelineState(ID3D12CommandAllocator* commandAllocator, ID3D12GraphicsCommandList* commandList);

	void BindTexture(ID3D12Device* device, std::string name, class Texture* texture);
	void BindConstantBuffer(std::string name, class ConstantBuffer* constantBuffer);

	void Release();

    ID3D12PipelineState* PipelineState;
	ID3D12DescriptorHeap* DescriptorHeap;

	VertexShader* VShader;
	PixelShader* PShader;

	DynamicRootSignature* RootSignature;

	std::map<uint32_t, D3D12_GPU_VIRTUAL_ADDRESS> ConstantBufferAddresses;

	std::vector<ID3D12DescriptorHeap*> DescriptorHeaps;
	std::map<std::string, uint32_t> HeapIndexMap;

};
