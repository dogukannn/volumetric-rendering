#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>
#include <fstream>
#include <vector>
#include <glm/matrix.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "pch.h"
#include "Mesh.h"
#include "Shader.h"

// Global variables for the window and DirectX
SDL_Window* GWindow = nullptr;
HWND GWindowHandle = nullptr;

int LoadImageDataFromFile(BYTE** imageData, D3D12_RESOURCE_DESC& resourceDescription, LPCWSTR filename, UINT64& bytesPerRow)
{
	static IWICImagingFactory2 *wicFactory;

    // reset decoder, frame and converter since these will be different for each image we load
    IWICBitmapDecoder *wicDecoder = NULL;
    IWICBitmapFrameDecode *wicFrame = NULL;
    IWICFormatConverter *wicConverter = NULL;

    if(wicFactory == NULL)
    {
		CoInitialize(NULL);
		ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory2, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory)));
    }

    ThrowIfFailed(wicFactory->CreateDecoderFromFilename(
        filename,
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &wicDecoder));

    ThrowIfFailed(wicDecoder->GetFrame(0, &wicFrame));

    WICPixelFormatGUID pixelFormat;
    ThrowIfFailed(wicFrame->GetPixelFormat(&pixelFormat));

    UINT textureWidth, textureHeight;
    ThrowIfFailed(wicFrame->GetSize(&textureWidth, &textureHeight));

    DXGI_FORMAT dxgiFormat = GetDXGIFormatFromWICFormat(pixelFormat);

	if(DXGI_FORMAT_UNKNOWN)
	{
		//TODO convert the image https://www.braynzarsoft.net/viewtutorial/q16390-directx-12-textures-from-file
	}

    UINT64 bitsPerPixel = GetDXGIFormatBitsPerPixel(dxgiFormat);
    bytesPerRow = (textureWidth * bitsPerPixel) / 8;
    UINT64 imageSize = bytesPerRow * textureHeight;

    *imageData = (BYTE*)malloc(imageSize);

    ThrowIfFailed(wicFrame->CopyPixels(0, bytesPerRow, imageSize, *imageData));

    resourceDescription = {};
    resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDescription.Alignment = 0;
    resourceDescription.Width = textureWidth;
    resourceDescription.Height = textureHeight;
    resourceDescription.DepthOrArraySize = 1;
    resourceDescription.MipLevels = 1;
    resourceDescription.Format = dxgiFormat;
    resourceDescription.SampleDesc.Count = 1;
    resourceDescription.SampleDesc.Quality = 0;
    resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

    return imageSize;
}


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
    //ID3D12GraphicsCommandList* commandList;
    //D3D12_RESOURCE_BARRIER barrier = {};
    //barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    //barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

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



    //create texture
    ID3D12Resource* textureBuffer;
    ID3D12Resource* textureBufferUploadHeap;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_RESOURCE_DESC textureDesc;
    UINT64 imageBytesPerRow;
    BYTE* imageData;
    UINT64 imageSize = LoadImageDataFromFile(&imageData, textureDesc, L"../Assets/lost_empire-RGBA.png", imageBytesPerRow);

    ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&textureBuffer)));
    textureBuffer->SetName(L"Texture Buffer Resource Heap");

    UINT64 textureUploadBufferSize;
    device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

    ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureBufferUploadHeap)));
    textureBuffer->SetName(L"Texture Buffer Upload Resource Heap");

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = &imageData[0];
    textureData.RowPitch = imageBytesPerRow;
    textureData.SlicePitch = imageBytesPerRow * textureDesc.Height;


	ID3D12GraphicsCommandList* uploadCommandList;
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
											commandAllocator, nullptr,
											IID_PPV_ARGS(&uploadCommandList)));
    UpdateSubresources(uploadCommandList, textureBuffer, textureBufferUploadHeap, 0, 0, 1, &textureData);
    uploadCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
    uploadCommandList->Close();

    ID3D12CommandList* ppCommandLists[] = {uploadCommandList};
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);


    ID3D12RootSignature* rootSignature;

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

	D3D12_DESCRIPTOR_RANGE1 ranges[2];
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors = 1;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;
    ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

    ranges[1].BaseShaderRegister = 1;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].NumDescriptors = 1;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;


    D3D12_ROOT_PARAMETER1 rootParameters[1];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 2;
    rootParameters[0].DescriptorTable.pDescriptorRanges = ranges;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.Desc_1_1.NumParameters = 1;
    rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
    rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
    rootSignatureDesc.Desc_1_1.pStaticSamplers = &sampler;

    ID3DBlob* signature;
    ID3DBlob* error;

    try
    {
        ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
        ThrowIfFailed(
            device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
        rootSignature->SetName(L"Hello Triangle Root Signature");
    }
    catch (std::exception e)
    {
        const char* errStr = (const char*)error->GetBufferPointer();
        std::cout << errStr;
        error->Release();
        error = nullptr;
    }

    if(signature)
    {
        signature->Release();
        signature = nullptr;
    }
    
    // Vertex data for the triangle
    Vertex vertexBufferData[3] =
    {
        { {0.0f, 0.5f, 0.0f}, {0.f, 0.f, 0.f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.f} },
        { {0.5f, -0.5f, 0.0f}, {0.f, 0.f, 0.f}, {0.0f, 1.0f, 0.0f}, {0.02f, 1.f} },
        { {-0.5f, -0.5f, 0.0f},{0.f, 0.f, 0.f}, {0.0f, 0.0f, 1.0f}, {0.98f, 1.f} }
    };

    Mesh mesh;
    mesh.loadFromObj("../Assets/lost_empire.obj");

    ID3D12Resource* vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

    const UINT vertexBufferSize = mesh._vertices.size() * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC vertexBufferResourceDesc;
    vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexBufferResourceDesc.Alignment = 0;
    vertexBufferResourceDesc.Width = vertexBufferSize;
    vertexBufferResourceDesc.Height = 1;
    vertexBufferResourceDesc.DepthOrArraySize = 1;
    vertexBufferResourceDesc.MipLevels = 1;
    vertexBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    vertexBufferResourceDesc.SampleDesc.Count = 1;
    vertexBufferResourceDesc.SampleDesc.Quality = 0;
    vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    vertexBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertexBufferResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer)));

    UINT8* pVertexDataBegin;

    D3D12_RANGE readRange;
    readRange.Begin = 0;
    readRange.End = 0;


    ThrowIfFailed(vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, mesh._vertices.data(), vertexBufferSize);
    vertexBuffer->Unmap(0, nullptr);

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = vertexBufferSize;


	uint32_t indexBufferData[3] = {0, 1, 2};

	ID3D12Resource* indexBuffer;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	const UINT indexBufferSize = sizeof(indexBufferData);

	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexBufferResourceDesc.Alignment = 0;
	vertexBufferResourceDesc.Width = indexBufferSize;
	vertexBufferResourceDesc.Height = 1;
	vertexBufferResourceDesc.DepthOrArraySize = 1;
	vertexBufferResourceDesc.MipLevels = 1;
	vertexBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	vertexBufferResourceDesc.SampleDesc.Count = 1;
	vertexBufferResourceDesc.SampleDesc.Quality = 0;
	vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	vertexBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(device->CreateCommittedResource(
    &heapProps, D3D12_HEAP_FLAG_NONE, &vertexBufferResourceDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer)));

	UINT8* pIndexDataBegin;

	D3D12_RANGE indexReadRange;
	indexReadRange.Begin = 0;
	indexReadRange.End = 0;

	ThrowIfFailed(indexBuffer->Map(0, &indexReadRange,
								   reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, indexBufferData, sizeof(indexBufferData));
	indexBuffer->Unmap(0, nullptr);

	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	indexBufferView.SizeInBytes = indexBufferSize;

	struct
	{
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
	} cbVS;

    cbVS.projectionMatrix = glm::perspective(glm::radians(45.f), 1.33f, 0.01f, 1000.f);
    glm::vec3 eye(25.2203f, 44.637f, -12.9169f);
    glm::vec3 eye_dir(-0.409304f, -0.27564f, 0.896766f);
    glm::vec3 up(0.f, 1.f, 0.f);
    cbVS.viewMatrix = glm::lookAt(eye, eye + eye_dir, up);
    cbVS.modelMatrix = glm::mat4(1.f);


	ID3D12Resource* constantBuffer;
	UINT8* mappedConstantBuffer;

	ID3D12DescriptorHeap* mainDescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.NumDescriptors = 2;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(device->CreateDescriptorHeap(&descHeapDesc,
											   IID_PPV_ARGS(&mainDescriptorHeap)));
	mainDescriptorHeap->SetName(L"Descriptor Heap For CBV + SRV");


	D3D12_HEAP_PROPERTIES cbHeapProperties;
	cbHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	cbHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	cbHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	cbHeapProperties.CreationNodeMask = 1;
	cbHeapProperties.VisibleNodeMask = 1;


	D3D12_RESOURCE_DESC cbResourceDesc;
	cbResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		frameIndex = swapchain->GetCurrentBackBufferIndex();
	cbResourceDesc.Alignment = 0;
	cbResourceDesc.Width = (sizeof(cbVS) + 255) & ~255;
	cbResourceDesc.Height = 1;
	cbResourceDesc.DepthOrArraySize = 1;
	cbResourceDesc.MipLevels = 1;
	cbResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	cbResourceDesc.SampleDesc.Count = 1;
	cbResourceDesc.SampleDesc.Quality = 0;
	cbResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	cbResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(device->CreateCommittedResource(
		&cbHeapProperties, D3D12_HEAP_FLAG_NONE, &cbResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer)));

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (sizeof(cbVS) + 255) & ~255; // CB size is required to be 256-byte aligned.

	D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	cbvHandle.ptr = cbvHandle.ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 0;

	device->CreateConstantBufferView(&cbvDesc, cbvHandle);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srvHandle.ptr = srvHandle.ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 1;

	device->CreateShaderResourceView(textureBuffer, &srvDesc, srvHandle);


	D3D12_RANGE readRange3;
	readRange3.Begin = 0;
	readRange3.End = 0;

	ThrowIfFailed(constantBuffer->Map(
		0, &readRange3, reinterpret_cast<void**>(&mappedConstantBuffer)));
	memcpy(mappedConstantBuffer, &cbVS, sizeof(cbVS));
	//constantBuffer->Unmap(0, &readRange3);

    VertexShader triangleVertexShader(L"../Assets/triangle.vert.hlsl");
    PixelShader trianglePixelShader(L"../Assets/triangle.px.hlsl");

	//D3D12_SHADER_BYTECODE vsBytecode;
	//std::string compiledVSPath = "../Assets/triangle.vert.dxil";

	//std::vector<char> vsBytecodeData = readFile(compiledVSPath);
	//vsBytecode.pShaderBytecode = vsBytecodeData.data();
	//vsBytecode.BytecodeLength = vsBytecodeData.size();

	//D3D12_SHADER_BYTECODE psBytecode;
	//std::string compiledPSPath = "../Assets/triangle.px.dxil";

	//std::vector<char> psBytecodeData = readFile(compiledPSPath);
	//psBytecode.pShaderBytecode = psBytecodeData.data();
	//psBytecode.BytecodeLength = psBytecodeData.size();

    ID3D12PipelineState* pipelineState;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.InputLayout = {Vertex::Description, _countof(Vertex::Description)};
	psoDesc.pRootSignature = rootSignature;

	psoDesc.VS = triangleVertexShader.GetShaderByteCode();
	psoDesc.PS = trianglePixelShader.GetShaderByteCode();

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
			&psoDesc, IID_PPV_ARGS(&pipelineState)));
	}
	catch (com_exception e)
	{
		std::cout << "Failed to create Graphics Pipeline! " << e.what();
	}

	//try
	//{
	//}
	//catch (com_exception e)
	//{
	//	std::cout << "Failed to record commands! " << e.what();
	//}

	ID3D12GraphicsCommandList* commandList;
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
											commandAllocator, pipelineState,
											IID_PPV_ARGS(&commandList)));
    commandList->Close();


	frameIndex = swapchain->GetCurrentBackBufferIndex();
    SDL_Event event;
    bool quit = false;

    float speed = 0.1f;
    static const glm::vec3 forward(0.f,0.f,1.f);
    static const glm::vec3 right(-1.f,0.f,0.f);
    bool captureDir = false;
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

        
		cbVS.viewMatrix = glm::lookAt(eye, eye + eye_dir, up);

        //std::cout << "eye " << eye.x << " " << eye.y << " " << eye.z << std::endl;
        //std::cout << "eye dir " << eye_dir.x << " " << eye_dir.y << " " << eye_dir.z << std::endl;
        //std::cout << "up " << up.x << " " << up.y << " " << up.z << std::endl;
        //std::cout << "==========================================================" << std::endl;
		//cbVS.modelMatrix = glm::rotate(cbVS.modelMatrix, glm::radians(1.0f), glm::vec3(0.f, 1.f, 0.f));
		memcpy(mappedConstantBuffer, &cbVS, sizeof(cbVS));

		ThrowIfFailed(commandAllocator->Reset());

		ThrowIfFailed(commandList->Reset(commandAllocator, pipelineState));

		commandList->SetGraphicsRootSignature(rootSignature);

		ID3D12DescriptorHeap* pDescriptorHeaps[] = {mainDescriptorHeap};
		commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

		D3D12_GPU_DESCRIPTOR_HANDLE cbvHandle2(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		commandList->SetGraphicsRootDescriptorTable(0, cbvHandle2);

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
        //commandList->OMSetRenderTargets(1, &rtvHandle2, FALSE, nullptr);


        commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                                           D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &surfaceSize);
		commandList->ClearRenderTargetView(rtvHandle2, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		//commandList->IASetIndexBuffer(&indexBufferView);
        commandList->DrawInstanced(mesh._vertices.size(), 1, 0, 0);

		//commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);

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