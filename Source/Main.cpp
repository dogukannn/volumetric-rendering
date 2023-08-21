#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>
#include <fstream>
#include <vector>

#define GLM_DEPTH_ZERO_TO_ONE
#include <chrono>
#include <glm/matrix.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ConstantBuffer.h"
#include "DynamicRootSignature.h"
#include "pch.h"
#include "Mesh.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Texture.h"

// Global variables for the window and DirectX
SDL_Window* GWindow = nullptr;
HWND GWindowHandle = nullptr;


// Create and initialize the window using SDL
bool InitializeWindow(int width, int height)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cout << "Failed to initialize SDL" << std::endl;
        return false;
    }

	SDL_SetRelativeMouseMode(SDL_TRUE);

    // Create window
    GWindow = SDL_CreateWindow("DirectX12 Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    if (GWindow == nullptr)
    {
        std::cout << "Failed to create SDL window" << std::endl;
        return false;
    }
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(GWindow, &wmInfo);
    GWindowHandle = wmInfo.info.win.window;

    //SDL_SetWindowGrab(GWindow, SDL_TRUE);
	return true;
}

void CopyTexturePlain(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES dstState, ID3D12Resource* dstTexture, D3D12_RESOURCE_STATES srcState, ID3D12Resource* srcTexture)
{
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(srcTexture, srcState, D3D12_RESOURCE_STATE_COPY_SOURCE));
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dstTexture, dstState, D3D12_RESOURCE_STATE_COPY_DEST));

	D3D12_BOX box;
	box.right = dstTexture->GetDesc().Width;
	box.bottom = dstTexture->GetDesc().Height;
	box.back = 1;
	box.front = 0;
	box.left = 0;
	box.top = 0;

	D3D12_TEXTURE_COPY_LOCATION destLoc = {};
	destLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	destLoc.pResource = dstTexture;
	destLoc.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
	srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcLoc.pResource = srcTexture;
	srcLoc.SubresourceIndex = 0;

	commandList->CopyTextureRegion(&destLoc, 0, 0, 0, &srcLoc, &box);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(srcTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, srcState));
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dstTexture, D3D12_RESOURCE_STATE_COPY_DEST, dstState));
}

inline std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    bool exists = (bool)file;

    if (!exists || !file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
};

int main(int argc, char* argv[]) 
{
    const int windowWidth = 800;
    const int windowHeight = 600;

    if (!InitializeWindow(windowWidth, windowHeight))
    {
        return 1;
    }

	ID3D12Debug* debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();

    // Select DirectX12 Physical Adapter
    IDXGIFactory4* factory = nullptr;
    ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));


    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
        adapter->Release();
    }

    // Create DirectX12 device
    ID3D12Device* device = nullptr;
    ThrowIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
    
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);
    
    std::cout << "DirectX12 device created" << std::endl;
    std::wcout << "Adapter: " << desc.Description << std::endl;
    std::cout << "Vendor ID: " << desc.VendorId << std::endl;
    std::cout << "Device ID: " << desc.DeviceId << std::endl;
    std::cout << "Dedicated Video Memory: " << desc.DedicatedVideoMemory << std::endl;

    // Create the command queue
    ID3D12CommandQueue* commandQueue;
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

    ID3D12CommandAllocator* commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));

    UINT frameIndex = 0;
    HANDLE fenceEvent;
    ID3D12Fence* fence;
    UINT64 fenceValue = 0;

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	 fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

    static const UINT backbufferCount = 2;
    UINT currentBuffer;
    ID3D12DescriptorHeap* renderTargetViewHeap;
    ID3D12Resource* renderTargets[backbufferCount];
    UINT rtvDescriptorSize;

    IDXGISwapChain1* swapchain1;
    IDXGISwapChain3* swapchain;

    D3D12_VIEWPORT viewport;
    D3D12_RECT surfaceSize;

    surfaceSize.left = 0;
    surfaceSize.top = 0;
    surfaceSize.right = windowWidth;
    surfaceSize.bottom = windowHeight;

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = windowWidth;
    viewport.Height = windowHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.BufferCount = backbufferCount;
    swapchainDesc.Width = windowWidth;
    swapchainDesc.Height = windowHeight;
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.SampleDesc.Count = 1;

    ThrowIfFailed(factory->CreateSwapChainForHwnd(commandQueue, GWindowHandle, &swapchainDesc, NULL, NULL, &swapchain1));

    ThrowIfFailed(swapchain1->QueryInterface(IID_PPV_ARGS(&swapchain)));

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = backbufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&renderTargetViewHeap)));

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());

    for (UINT n = 0; n < backbufferCount; n++)
    {
        ThrowIfFailed(swapchain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
        device->CreateRenderTargetView(renderTargets[n], nullptr, rtvHandle);
        rtvHandle.ptr += (1 * rtvDescriptorSize);
    }

    ID3D12DescriptorHeap* sideRenderTargetViewHeap;
    ID3D12Resource* backDepthRenderTargets[backbufferCount];
    ID3D12Resource* frontDepthRenderTargets[backbufferCount];

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC sideRTDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				DXGI_FORMAT_R32_FLOAT,
				windowWidth,
				windowHeight,
				1,
				1,
				1);
    sideRTDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE sideRTClearValue = {};
    sideRTClearValue.Format = DXGI_FORMAT_R32_FLOAT;
    sideRTClearValue.Color[0] = 0.0f;

    for(int i = 0; i < backbufferCount; i++)
    {
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&sideRTDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&sideRTClearValue,
			IID_PPV_ARGS(&backDepthRenderTargets[i])
		));
		backDepthRenderTargets[i]->SetName(L"back depth write targets");

		ThrowIfFailed(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&sideRTDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&sideRTClearValue,
			IID_PPV_ARGS(&frontDepthRenderTargets[i])
		));
		frontDepthRenderTargets[i]->SetName(L"back depth write targets");
    }

    D3D12_DESCRIPTOR_HEAP_DESC sideRtvHeapDesc = {};
    sideRtvHeapDesc.NumDescriptors = backbufferCount * 2;
    sideRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    sideRtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&sideRtvHeapDesc, IID_PPV_ARGS(&sideRenderTargetViewHeap)));

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE sideRtvHandle(sideRenderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());

    for (UINT n = 0; n < backbufferCount; n++)
    {
        device->CreateRenderTargetView(backDepthRenderTargets[n], nullptr, sideRtvHandle);
        sideRtvHandle.ptr += (1 * rtvDescriptorSize);
    }
    for (UINT n = 0; n < backbufferCount; n++)
    {
        device->CreateRenderTargetView(frontDepthRenderTargets[n], nullptr, sideRtvHandle);
        sideRtvHandle.ptr += (1 * rtvDescriptorSize);
    }
    
	//create depth stencil
    ID3D12Resource* depthStencilBuffer;
    ID3D12DescriptorHeap* dsDescriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap)));

    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, windowWidth, windowHeight, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&depthStencilBuffer)
    ));
	dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");

	device->CreateDepthStencilView(depthStencilBuffer, &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


    // Vertex data for the triangle
    Vertex vertexBufferData[3] =
    {
        { {0.0f, 0.5f, 0.0f}, {0.f, 0.f, 0.f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.f} },
        { {0.5f, -0.5f, 0.0f}, {0.f, 0.f, 0.f}, {0.0f, 1.0f, 0.0f}, {0.02f, 1.f} },
        { {-0.5f, -0.5f, 0.0f},{0.f, 0.f, 0.f}, {0.0f, 0.0f, 1.0f}, {0.98f, 1.f} }
    };

    Mesh mesh;
    mesh.loadFromObj(device, "../Assets/graveyard.obj");

    Mesh cubeMesh;
    cubeMesh.loadFromObj(device, "../Assets/cube.obj");

    Vertex a = { {-3.0f, -1.0f, 0.0f}, {3.f, 3.f, 3.f}, {3.f, 3.f, 3.f}, {3.f, 3.f} };
    Vertex b = { {1.0f, -1.0f, 0.0f}, {3.f, 3.f, 3.f}, {3.f, 3.f, 3.f}, {3.f, 3.f} };
    Vertex c = { {1.0f, 3.0f, 0.0f}, {3.f, 3.f, 3.f}, {3.f, 3.f, 3.f}, {3.f, 3.f} };
    std::vector<Vertex> tri = { a, b, c };
    Mesh triangle;
    triangle.loadFromVertices(device, tri);

	struct mvp
	{
		glm::mat4 mvpmat;
		glm::mat4 inverseVP;
        glm::vec3 eye;
        float time;
	} cbVS;

    auto projectionMatrix = glm::perspective(glm::radians(45.f), 1.33f, 1.0f, 1000.f);

	//projectionMatrix = glm::translate(glm::mat4(1.f), glm::vec3(0.0f,0.0f,0.5f)) * glm::scale(glm::mat4(1.f), glm::vec3(1.0f,1.0f,0.5f)) * projectionMatrix;
    glm::vec3 eye(25.2203f, 44.637f, -12.9169f);
    glm::vec3 eye_dir(-0.409304f, -0.27564f, 0.896766f);
    glm::vec3 up(0.f, 1.f, 0.f);
    auto viewMatrix = glm::lookAt(eye, eye + eye_dir, up);
    auto modelMatrix = glm::mat4(1.f);
    cbVS.mvpmat = projectionMatrix * viewMatrix * modelMatrix;

    VertexShader triangleVertexShader(L"../Assets/triangle.vert.hlsl");
    PixelShader trianglePixelShader(L"../Assets/triangle.px.hlsl");
    PixelShader depthPixelShader(L"../Assets/depth_save.px.hlsl");

	VertexShader noopVertexShader(L"../Assets/noop.vert.hlsl");
	PixelShader volumePixelShader(L"../Assets/volumetric.px.hlsl");

	Pipeline pipeline;
	pipeline.Initialize(device, &triangleVertexShader, &trianglePixelShader);

	Pipeline depthBackPipeline;
    depthBackPipeline.CullMode = D3D12_CULL_MODE_BACK;
    depthBackPipeline.writeDepth = false;
	depthBackPipeline.Initialize(device, &triangleVertexShader, &depthPixelShader);

	Pipeline depthFrontPipeline;
    depthFrontPipeline.CullMode = D3D12_CULL_MODE_FRONT;
    depthFrontPipeline.writeDepth = false;
	depthFrontPipeline.Initialize(device, &triangleVertexShader, &depthPixelShader);

	Pipeline volumetricPipeline;
    volumetricPipeline.useAlphaBlend = true;
	volumetricPipeline.Initialize(device, &noopVertexShader, &volumePixelShader);

    ConstantBuffer sceneBuffer;
    sceneBuffer.Initialize(device, sizeof(cbVS));
    UINT8* sceneBufferMapped = sceneBuffer.Map();

    Texture texture;
    texture.LoadFromFile(device, commandQueue, L"../Assets/lost_empire-RGBA.png");

    pipeline.BindTexture(device, "g_texture", &texture);

	ID3D12GraphicsCommandList* commandList;
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
											commandAllocator, pipeline.PipelineState,
											IID_PPV_ARGS(&commandList)));
    commandList->Close();


	memcpy(sceneBufferMapped, &cbVS, sizeof(cbVS));

    ConstantBuffer cubeBuffer;
    cubeBuffer.Initialize(device, sizeof(mvp));
    UINT8* cubeBufferMapped = cubeBuffer.Map();
    mvp CubeMvp;
    auto CubeMvpprojectionMatrix = glm::perspective(glm::radians(45.f), 1.33f, 1.0f, 1000.f);
	//CubeMvpprojectionMatrix = glm::translate(glm::mat4(1.f), glm::vec3(0.0f,0.0f,0.5f)) * glm::scale(glm::mat4(1.f), glm::vec3(1.0f,1.0f,0.5f)) * CubeMvpprojectionMatrix;
    auto CubeMvpviewMatrix = glm::lookAt(eye, eye + eye_dir, up);
    auto CubeMvpmodelMatrix = glm::scale(glm::mat4(1.f),glm::vec3(4.f));
    CubeMvp.mvpmat = CubeMvpprojectionMatrix * CubeMvpviewMatrix * CubeMvpmodelMatrix;
    CubeMvp.inverseVP = glm::inverse(CubeMvpprojectionMatrix* CubeMvpviewMatrix);
    CubeMvp.eye = eye;
	memcpy(cubeBufferMapped, &CubeMvp, sizeof(mvp));

	std::chrono::time_point<std::chrono::system_clock> startTime;
	startTime = std::chrono::system_clock::now();

	frameIndex = swapchain->GetCurrentBackBufferIndex();
    SDL_Event event;
    bool quit = false;

    float speed = 0.1f;
    static const glm::vec3 forward(0.f,0.f,1.f);
    static const glm::vec3 right(-1.f,0.f,0.f);
    bool captureDir = false;
    eye = glm::vec3(8.0f, 0.0f, 0.0f);
    eye_dir = glm::vec3(-1.0f, 0.0f, 0.0f);
    while (!quit)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                quit = true;
            }
            if (event.type == SDL_KEYDOWN)
            {
	            switch (event.key.keysym.sym)
	            {
					case SDLK_w:
						eye += eye_dir * speed;
						break;
	            case SDLK_d:
						eye += glm::cross(eye_dir, up) * speed;
						break;
	            	case SDLK_s:

						eye -= eye_dir * speed;
						break;
					case SDLK_a:
						eye -= glm::cross(eye_dir, up) * speed;
						break;
					case SDLK_r:
						eye = glm::vec3(0.f, 0.f, -3.f);
						eye_dir =  glm::vec3(0.f,0.f,1.f);
						up =  glm::vec3(0.f, 1.f, 0.f);
                        captureDir = false;
						break;

	            }
            }
            if(event.type == SDL_MOUSEBUTTONDOWN)
            {
                captureDir = true;
            }
		}
        if(captureDir)
        {
			int x, y;
            static float yaw = 0.0f, pitch = 0.0f;
			SDL_GetRelativeMouseState(&x, &y);
            yaw += x * 0.2f;
            pitch += y * 0.2f;
            glm::vec3 direction;
			direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
			direction.y = sin(glm::radians(pitch));
			direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            eye_dir = direction;
        }

        
		viewMatrix = glm::lookAt(eye, eye + eye_dir, up);
		cbVS.mvpmat = projectionMatrix * viewMatrix * modelMatrix;
        
		CubeMvpviewMatrix = glm::lookAt(eye, eye + eye_dir, up);
		CubeMvp.mvpmat = CubeMvpprojectionMatrix * CubeMvpviewMatrix * CubeMvpmodelMatrix;
		CubeMvp.inverseVP = glm::inverse(CubeMvpprojectionMatrix * CubeMvpviewMatrix);
        CubeMvp.eye = eye;

		auto now = std::chrono::system_clock::now();
		std::chrono::duration<float, std::ratio<1,1>> diff = now - startTime;
		CubeMvp.time = diff.count();

		//std::cerr << "\r" << static_cast<int>((static_cast<double>(imageHeight - j) / imageHeight) * 100.0) << "% of file write is completed         " << std::flush;
        //std::cout << "eye " << eye.x << " " << eye.y << " " << eye.z << std::endl;
        //std::cout << "eye dir " << eye_dir.x << " " << eye_dir.y << " " << eye_dir.z << std::endl;
        //std::cout << "up " << up.x << " " << up.y << " " << up.z << std::endl;
        //std::cout << "==========================================================" << std::endl;

		memcpy(sceneBufferMapped, &cbVS, sizeof(cbVS));
		memcpy(cubeBufferMapped, &CubeMvp, sizeof(mvp));

		ThrowIfFailed(commandAllocator->Reset());

		ThrowIfFailed(commandList->Reset(commandAllocator, nullptr));

        pipeline.SetPipelineState(commandAllocator, commandList);

		D3D12_RESOURCE_BARRIER renderTargetBarrier;
		renderTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		renderTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		renderTargetBarrier.Transition.pResource = renderTargets[frameIndex];
		renderTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		renderTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		renderTargetBarrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &renderTargetBarrier);

		D3D12_CPU_DESCRIPTOR_HANDLE
			rtvHandle2(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());
		rtvHandle2.ptr = rtvHandle2.ptr + (frameIndex * rtvDescriptorSize);

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        commandList->OMSetRenderTargets(1, &rtvHandle2, FALSE, &dsvHandle);

        commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                                           D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &surfaceSize);
		commandList->ClearRenderTargetView(rtvHandle2, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pipeline.BindConstantBuffer("cb", &sceneBuffer, commandList);
		commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
		//commandList->IASetIndexBuffer(&indexBufferView);
        commandList->DrawInstanced(mesh._vertices.size(), 1, 0, 0);


		D3D12_CPU_DESCRIPTOR_HANDLE
			rtvHandle3(sideRenderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());
		rtvHandle3.ptr = rtvHandle3.ptr + (frameIndex * rtvDescriptorSize);

		const float clearColorx[] = {0.0f, 0.0f, 0.0f, 0.0f};
		commandList->ClearRenderTargetView(rtvHandle3, clearColorx, 0, nullptr);
        
        CopyTexturePlain(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET, backDepthRenderTargets[frameIndex], D3D12_RESOURCE_STATE_DEPTH_WRITE, depthStencilBuffer);
		
        commandList->OMSetRenderTargets(1, &rtvHandle3, FALSE, &dsvHandle);

        depthBackPipeline.SetPipelineState(commandAllocator, commandList);
    	depthBackPipeline.BindConstantBuffer("cb", &cubeBuffer, commandList);
		commandList->IASetVertexBuffers(0, 1, &cubeMesh.vertexBufferView);
        commandList->DrawInstanced(cubeMesh._vertices.size(), 1, 0, 0);

		D3D12_CPU_DESCRIPTOR_HANDLE
			rtvHandle4(sideRenderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());
		rtvHandle4.ptr = rtvHandle4.ptr + ((frameIndex + backbufferCount) * rtvDescriptorSize);

		commandList->ClearRenderTargetView(rtvHandle4, clearColorx, 0, nullptr);
        
        commandList->OMSetRenderTargets(1, &rtvHandle4, FALSE, &dsvHandle);
        depthFrontPipeline.SetPipelineState(commandAllocator, commandList);
    	depthFrontPipeline.BindConstantBuffer("cb", &cubeBuffer, commandList);
		commandList->IASetVertexBuffers(0, 1, &cubeMesh.vertexBufferView);
        commandList->DrawInstanced(cubeMesh._vertices.size(), 1, 0, 0);


		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backDepthRenderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(frontDepthRenderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
        
        commandList->OMSetRenderTargets(1, &rtvHandle2, FALSE, &dsvHandle);
        commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                                           D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        volumetricPipeline.SetPipelineState(commandAllocator, commandList);
    	volumetricPipeline.BindConstantBuffer("cb", &cubeBuffer, commandList);
    	volumetricPipeline.BindTexture(device, "frontCulled", backDepthRenderTargets[frameIndex]);
    	volumetricPipeline.BindTexture(device, "backCulled", frontDepthRenderTargets[frameIndex]);
		commandList->IASetVertexBuffers(0, 1, &triangle.vertexBufferView);
        commandList->DrawInstanced(triangle._vertices.size(), 1, 0, 0);


		D3D12_RESOURCE_BARRIER presentBarrier;
		presentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		presentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		presentBarrier.Transition.pResource = renderTargets[frameIndex];
		presentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		presentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		presentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		commandList->ResourceBarrier(1, &presentBarrier);

		ThrowIfFailed(commandList->Close());

		ID3D12CommandList* ppCommandLists[] = {commandList};
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		 swapchain->Present(1, 0);

		// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.

		// Signal and increment the fence value.
		ThrowIfFailed(commandQueue->Signal(fence, fenceValue++));

		// Wait until the previous frame is finished.
		if (fence->GetCompletedValue() < fenceValue - 1)
		{
			ThrowIfFailed(fence->SetEventOnCompletion(fenceValue - 1, fenceEvent));
			WaitForSingleObject(fenceEvent, INFINITE);
		}

		frameIndex = swapchain->GetCurrentBackBufferIndex();

    }

    SDL_DestroyWindow(GWindow);
    SDL_Quit();

    // Cleanup
    commandQueue->Release();
    device->Release();
    adapter->Release();
    factory->Release();

    return 0;
}