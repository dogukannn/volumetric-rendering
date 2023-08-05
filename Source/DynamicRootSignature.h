#pragma once
#include "Shader.h"

class DynamicRootSignature
{
public:
	DynamicRootSignature();
	bool Initialize(ID3D12Device* device, VertexShader& vertexShader, PixelShader& pixelShader);
	
    ID3D12RootSignature* rootSignature;
	std::map<std::string, uint32_t> VertexParameterIndex;
	std::map<std::string, uint32_t> PixelParameterIndex;

	std::vector<D3D12_ROOT_PARAMETER1> rootParameters;
};
