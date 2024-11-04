#include <Renderer.h>
#include <Window.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <assert.h>

#define NUM_BACKBUFFERS 2

class RendererImpl
{
public:
    RendererImpl() = default;
    ~RendererImpl() = default;

    void CreateDevice();
    void CreateCommandQueue();
    void CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);

private:
    ID3D12Device* mDevice;
    ID3D12CommandQueue* mCommandQueue;
    IDXGISwapChain3* mSwapChain;

    ID3D12DescriptorHeap* mRtvHeap;
    uint32_t mRtvDescriptorSize;

    ID3D12Resource* mRenderTargets[NUM_BACKBUFFERS];

    uint32_t mFrameIndex;
};

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
}

void Renderer::Initialize(Window& window)
{
    mImpl.reset(new RendererImpl());
    mImpl->CreateDevice();
    mImpl->CreateCommandQueue();
    mImpl->CreateSwapChain(window.GetHandle(), window.GetWidth(), window.GetHeight());
}

void Renderer::Shutdown()
{
}

void Renderer::Render()
{
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
    assert(SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory))));
    assert(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice))));
    factory->Release();
}

void RendererImpl::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    assert(SUCCEEDED(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue))));
}

void RendererImpl::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height)
{
    IDXGIFactory4* factory;
    assert(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = NUM_BACKBUFFERS;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    IDXGISwapChain1* tempSwapChain;
    assert(SUCCEEDED(factory->CreateSwapChainForHwnd(mCommandQueue,
                                                     hwnd,
                                                     &swapChainDesc,
                                                     nullptr,
                                                     nullptr,
                                                     &tempSwapChain)));

    assert(SUCCEEDED(tempSwapChain->QueryInterface(IID_PPV_ARGS(&mSwapChain))));

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
        assert(SUCCEEDED(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap))));

        mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create the render target views
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (size_t i = 0; i < NUM_BACKBUFFERS; i++)
    {
        assert(SUCCEEDED(mSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (LPVOID*)&mRenderTargets[i])));
        mDevice->CreateRenderTargetView(mRenderTargets[i], NULL, rtv);
        rtv.ptr += mRtvDescriptorSize;
    }
}