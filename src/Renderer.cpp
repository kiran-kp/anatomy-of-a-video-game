#include <Renderer.h>
#include <Log.h>
#include <Util.h>
#include <Window.h>

// This helper library has to be included before any SDK headers
#include <directx/d3dx12.h>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxcapi.h>
#include <dxgi1_4.h>

// Double buffer so we can continue doing work on the CPU while GPU renders the previous frame.
// Using a #define here because this is used to set uint32_t or size_t in different contexts and I didn't want cast it every time.
#define NUM_BACKBUFFERS 2

class TriangleRenderer
{
public:
    TriangleRenderer() = default;
    ~TriangleRenderer();

    void Initialize(ID3D12Device* device, float width, float height);

    void Render(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex);

private:

    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };

    CD3DX12_VIEWPORT mViewport;
    CD3DX12_RECT mScissorRect;

    ID3D12RootSignature* mRootSignature;
    ID3D12PipelineState* mPipelineState;
    ID3D12Resource* mVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
};

class RendererImpl
{
public:
    RendererImpl() = default;
    ~RendererImpl() = default;

    void CreateDevice();
    void CreateCommandQueue();
    void CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
    void CreateCommandList();
    void CreateFence();

    void InitializeTriangleRenderer();

    void PopulateCommandListAndSubmit();
    void Present();
    void WaitForPreviousFrame();

private:
    ID3D12Device* mDevice;
    ID3D12CommandQueue* mCommandQueue;
    IDXGISwapChain3* mSwapChain;
    ID3D12CommandAllocator* mCommandAllocator;

    uint32_t mWidth;
    uint32_t mHeight;

    ID3D12DescriptorHeap* mRtvHeap;
    uint32_t mRtvDescriptorSize;

    ID3D12Resource* mRenderTargets[NUM_BACKBUFFERS];
    ID3D12GraphicsCommandList* mCommandList;

    TriangleRenderer mTriangleRenderer;

    uint32_t mFrameIndex;
    ID3D12Fence* mFence;
    HANDLE mFenceEvent;
    uint64_t mFenceValue;
};

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::Initialize(Window& window)
{
    mImpl.reset(new RendererImpl());
    mImpl->CreateDevice();
    mImpl->CreateCommandQueue();
    mImpl->CreateSwapChain(window.GetHandle(), window.GetWidth(), window.GetHeight());
    mImpl->CreateCommandList();
    mImpl->CreateFence();

    mImpl->InitializeTriangleRenderer();

    // Wait for all the setup work we just did to complete because we are going to re-use the command list
    mImpl->WaitForPreviousFrame();
}

void Renderer::Shutdown()
{
}

void Renderer::Render()
{
    mImpl->PopulateCommandListAndSubmit();
    mImpl->Present();
    mImpl->WaitForPreviousFrame();
}

void RendererImpl::CreateDevice()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    {
        ID3D12Debug* debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            debugController->Release();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    IDXGIFactory4* factory;
    ensure(SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory))));
    ensure(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice))));
    factory->Release();
}

void RendererImpl::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ensure(SUCCEEDED(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue))));
}

void RendererImpl::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height)
{
    IDXGIFactory4* factory;
    ensure(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));

    mWidth = width;
    mHeight = height;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = NUM_BACKBUFFERS;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    IDXGISwapChain1* tempSwapChain;
    ensure(SUCCEEDED(factory->CreateSwapChainForHwnd(mCommandQueue,
                                                     hwnd,
                                                     &swapChainDesc,
                                                     nullptr,
                                                     nullptr,
                                                     &tempSwapChain)));

    ensure(SUCCEEDED(tempSwapChain->QueryInterface(IID_PPV_ARGS(&mSwapChain))));

    // Release the temporary swap chain
    tempSwapChain->Release();
    factory->Release();

    mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = NUM_BACKBUFFERS;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ensure(SUCCEEDED(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap))));

        mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create the render target views
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (size_t i = 0; i < NUM_BACKBUFFERS; i++)
    {
        ensure(SUCCEEDED(mSwapChain->GetBuffer(static_cast<uint32_t>(i), __uuidof(ID3D12Resource), (LPVOID*)&mRenderTargets[i])));
        mDevice->CreateRenderTargetView(mRenderTargets[i], NULL, rtv);
        rtv.ptr += mRtvDescriptorSize;
    }

    // Create a command allocator which will be used to allocate command lists
    mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator));
}

void RendererImpl::CreateCommandList()
{
    ensure(SUCCEEDED(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator, nullptr, IID_PPV_ARGS(&mCommandList))));

    // The command list is in the recording state when it is created but we have nothing do to at the moment.
    mCommandList->Close();
}

// A fence is a synchronization primitive that we can use to signal that the GPU is done rendering a frame
void RendererImpl::CreateFence()
{
    ensure(SUCCEEDED(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence))));
    mFenceValue = 1;

    // This is the event we will listen for
    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (mFenceEvent == nullptr)
    {
        ensure(SUCCEEDED(HRESULT_FROM_WIN32(GetLastError())));
    }
}

void RendererImpl::InitializeTriangleRenderer()
{
    mTriangleRenderer.Initialize(mDevice, static_cast<float>(mWidth), static_cast<float>(mHeight));
}

void RendererImpl::PopulateCommandListAndSubmit()
{
    // This should only be done after the command list associated with this allocator has finished execution.
    // Ensure that this is only called after WaitForPreviousFrame().
    ensure(SUCCEEDED(mCommandAllocator->Reset()));

    // This sets it back to the recording state so we can set up our frame
    ensure(SUCCEEDED(mCommandList->Reset(mCommandAllocator, nullptr)));

    // Transition our back buffer to be able to be used as a render target since we're rendering to it
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mFrameIndex, mRtvDescriptorSize);
    mCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    mTriangleRenderer.Render(mCommandList, mFrameIndex);

    // Transition back buffer back to the present state since we are done drawing to it and want it ready for present
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ensure(SUCCEEDED(mCommandList->Close()));

    ID3D12CommandList* ppCommandLists[] = { mCommandList };
    mCommandQueue->ExecuteCommandLists(1, ppCommandLists);
}

void RendererImpl::Present()
{
    ensure(SUCCEEDED(mSwapChain->Present(1, 0)));
}

void RendererImpl::WaitForPreviousFrame()
{
    // TODO: This is apparently not a good way to queue up work for the GPU because we're doing nothing while we wait for the previous frame to render.
    // Look into the frame buffering sample

    // Signal and increment the fence value.
    const UINT64 fence = mFenceValue;
    ensure(SUCCEEDED(mCommandQueue->Signal(mFence, fence)));
    mFenceValue++;

    // Wait until the previous frame is finished.
    if (mFence->GetCompletedValue() < fence)
    {
        ensure(SUCCEEDED(mFence->SetEventOnCompletion(fence, mFenceEvent)));
        WaitForSingleObject(mFenceEvent, INFINITE);
    }

    mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
}

TriangleRenderer::~TriangleRenderer()
{
    mRootSignature->Release();
    mPipelineState->Release();
    mVertexBuffer->Release();
}

void TriangleRenderer::Initialize(ID3D12Device* device, float width, float height)
{
    float aspectRatio = width / height;
    mViewport.TopLeftX = 0.0f;
    mViewport.TopLeftY = 0.0f;
    mViewport.Width = width;
    mViewport.Height = height;
    mViewport.MinDepth = D3D12_MIN_DEPTH;
    mViewport.MaxDepth = D3D12_MAX_DEPTH;

    mScissorRect.left = 0;
    mScissorRect.top = 0;
    mScissorRect.right= static_cast<uint64_t>(width);
    mScissorRect.bottom = static_cast<uint64_t>(height);


    // Create a root signature. This data structure describes what resources are bound to the pipeline at each shader stage.
    // In our case, we don't need anything yet.
    {
        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.NumParameters = 0;
        rootSignatureDesc.pParameters = nullptr;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* signature;
        ID3DBlob* error;
        ensure(SUCCEEDED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)));
        ensure(SUCCEEDED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature))));
    }

    // Create our pipeline
    {
        ID3DBlob* vertexShader;
        ID3DBlob* pixelShader;

#if defined(_DEBUG)
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        // Load the shader file. This assumes it's in the working directory when the game runs. Make sure to set it in debugging settings.
        ensure(SUCCEEDED(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr)));
        ensure(SUCCEEDED(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr)));

        // Define the layout for the vertex shader input.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Create the pipeline state object (PSO). This describes everything required to run this specific shader.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = mRootSignature;
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ensure(SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState))));
    }

    // Set up the vertex buffers here for now since this shader is very basic and not doing anything interesting
    {
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        const uint32_t vertexBufferSize = sizeof(triangleVertices);

        // TODO: Keeping this comment from sample code as is.
        // I don't understand what this means at the moment so we'll worry about best practice for uploading vertices to GPU later.

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        ensure(SUCCEEDED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                          D3D12_HEAP_FLAG_NONE,
                                                          &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
                                                          D3D12_RESOURCE_STATE_GENERIC_READ,
                                                          nullptr,
                                                          IID_PPV_ARGS(&mVertexBuffer))));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);
        // "Mapping" a GPU buffer gets us an address for the memory so we can read or write to it. We set read range to 0, 0 since we only want to write now.
        ensure(SUCCEEDED(mVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin))));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        mVertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
        mVertexBufferView.StrideInBytes = sizeof(Vertex);
        mVertexBufferView.SizeInBytes = vertexBufferSize;
    }
}

void TriangleRenderer::Render(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex)
{
    commandList->SetGraphicsRootSignature(mRootSignature);
    commandList->SetPipelineState(mPipelineState);
    commandList->RSSetViewports(1, &mViewport);
    commandList->RSSetScissorRects(1, &mScissorRect);

    // This is the actual stuff we are drawing
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    commandList->DrawInstanced(3, 1, 0, 0);
}