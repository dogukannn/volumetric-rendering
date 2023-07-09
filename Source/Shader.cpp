#include "Shader.h"

Shader::Shader()
{
}

D3D12_SHADER_BYTECODE Shader::GetShaderByteCode()
{
	D3D12_SHADER_BYTECODE bytecode = {};
	bytecode.BytecodeLength = ShaderBlob->GetBufferSize();
	bytecode.pShaderBytecode = ShaderBlob->GetBufferPointer();
	return bytecode;
}

VertexShader::VertexShader(LPCWSTR shaderFile): Shader()
{
	auto shaderCompiler = ShaderCompiler::GetInstance();
	shaderCompiler->CompileVertexShader(shaderFile, ShaderBlob);
}

PixelShader::PixelShader(LPCWSTR shaderFile): Shader()
{
	auto shaderCompiler = ShaderCompiler::GetInstance();
	shaderCompiler->CompilePixelShader(shaderFile, ShaderBlob);
}
