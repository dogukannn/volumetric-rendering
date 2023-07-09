#pragma once
#include "ShaderCompiler.h"

class Shader
{
protected:
	Shader();
public:
	std::string ShaderName;
	CComPtr<ID3DBlob> ShaderBlob;
	D3D12_SHADER_BYTECODE GetShaderByteCode();
};

class VertexShader : public Shader
{
public:
	VertexShader(LPCWSTR shaderFile);
};

class PixelShader : public Shader
{
public:
	PixelShader(LPCWSTR shaderFile);
};
