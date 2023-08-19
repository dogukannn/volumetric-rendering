#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "pch.h"


struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv;

	static inline D3D12_INPUT_ELEMENT_DESC Description[] = 
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(glm::vec3) ,
		 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(glm::vec3) * 2,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(glm::vec3) * 3,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
};

struct Mesh
{
	std::vector<Vertex> _vertices;

    ID3D12Resource* vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	bool loadFromObj(ID3D12Device* device, const char* filename);
	bool loadFromVertices(ID3D12Device* device, std::vector<Vertex>& vertices);
};