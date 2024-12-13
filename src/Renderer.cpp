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

#include <array>

// Using a #define here because this is used to set uint32_t or size_t in different contexts and I didn't want cast it every time.
#define NUM_BACKBUFFERS 2

class TriangleRenderer
{
public:
    TriangleRenderer() = default;
    ~TriangleRenderer();

    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, float width, float height);

    void Render(ID3D12GraphicsCommandList* commandList);

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

TriangleRenderer::~TriangleRenderer()
{
    mRootSignature->Release();
    mPipelineState->Release();
    mVertexBuffer->Release();
}

void TriangleRenderer::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, float width, float height)
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
        ensure(SUCCEEDED(D3DCompileFromFile(L"data/basic.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr)));
        ensure(SUCCEEDED(D3DCompileFromFile(L"data/basic.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr)));

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

void TriangleRenderer::Render(ID3D12GraphicsCommandList* commandList)
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

// ------------------------------------------------------------------------------------------------

class TexturedTriangleRenderer
{
public:
    TexturedTriangleRenderer() = default;
    ~TexturedTriangleRenderer();

    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, float width, float height);
    void Render(ID3D12GraphicsCommandList* commandList);

private:
    static constexpr uint32_t TextureWidth = 256;
    static constexpr uint32_t TextureHeight = 256;
    static constexpr uint32_t TexturePixelSize = 4;

    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 uv;
    };

    CD3DX12_VIEWPORT mViewport;
    CD3DX12_RECT mScissorRect;

    ID3D12RootSignature* mRootSignature;
    ID3D12PipelineState* mPipelineState;
    ID3D12Resource* mVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

    ID3D12DescriptorHeap* mSrvHeap;
    ID3D12Resource* mTexture;
};

TexturedTriangleRenderer::~TexturedTriangleRenderer()
{
    mRootSignature->Release();
    mPipelineState->Release();
    mVertexBuffer->Release();
    mTexture->Release();
}

std::vector<uint8_t> GenerateTextureData(const uint32_t textureWidth, const uint32_t textureHeight, const uint32_t texturePixelSize)
{
    const uint32_t rowPitch = textureWidth * texturePixelSize;
    const uint32_t cellPitch = rowPitch >> 3;
    const uint32_t cellHeight = textureWidth >> 3;
    const uint32_t textureSize = rowPitch * textureHeight;

    std::vector<uint8_t> data(textureSize);
    uint8_t* pData = &data[0];

    for (uint32_t n = 0; n < textureSize; n += texturePixelSize)
    {
        uint32_t x = n % rowPitch;
        uint32_t y = n / rowPitch;
        uint32_t i = x / cellPitch;
        uint32_t j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n] = 0x00;        // R
            pData[n + 1] = 0x00;    // G
            pData[n + 2] = 0x00;    // B
            pData[n + 3] = 0xff;    // A
        }
        else
        {
            pData[n] = 0xff;        // R
            pData[n + 1] = 0xff;    // G
            pData[n + 2] = 0xff;    // B
            pData[n + 3] = 0xff;    // A
        }
    }

    return data;
}

void TexturedTriangleRenderer::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, float width, float height)
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

    // Describe and create a shader resource view (SRV) heap for the texture.
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ensure(SUCCEEDED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap))));

    // Create root signature
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[1];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // 1 SRV at register t0

        CD3DX12_ROOT_PARAMETER rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

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
        sampler.ShaderRegister = 0;  // This matches s0 in the shader
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init((UINT)1, rootParameters, (UINT)1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ID3DBlob* signature;
        ID3DBlob* error;
        ensure(SUCCEEDED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)));
        ensure(SUCCEEDED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature))));
        
        signature->Release();
        if (error) error->Release();
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
        ensure(SUCCEEDED(D3DCompileFromFile(L"data/textured.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr)));
        ensure(SUCCEEDED(D3DCompileFromFile(L"data/textured.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr)));

        // Define the layout for the vertex shader input.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
            { { 0.0f, 0.25f * aspectRatio, 0.0f }, { 0.5f, 0.0f } },
            { { 0.25f, -0.25f * aspectRatio, 0.0f }, { 1.0f, 1.0f } },
            { { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f } }
        };

        const uint32_t vertexBufferSize = sizeof(triangleVertices);

        // Just using an upload heap directly for this since performance doesn't matter
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

    {
        // Describe and create a Texture2D.
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = TextureWidth;
        textureDesc.Height = TextureHeight;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        ensure(SUCCEEDED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                         D3D12_HEAP_FLAG_NONE,
                                                         &textureDesc,
                                                         D3D12_RESOURCE_STATE_COPY_DEST,
                                                         nullptr,
                                                         IID_PPV_ARGS(&mTexture))));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mTexture, 0, 1);

        // Create the GPU upload buffer.
        ID3D12Resource* textureUploadHeap;
        ensure(SUCCEEDED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                         D3D12_HEAP_FLAG_NONE,
                                                         &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                                                         D3D12_RESOURCE_STATE_GENERIC_READ,
                                                         nullptr,
                                                         IID_PPV_ARGS(&textureUploadHeap))));

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the Texture2D.
        std::vector<uint8_t> texture = GenerateTextureData(TextureWidth, TextureHeight, TexturePixelSize);

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &texture[0];
        textureData.RowPitch = TextureWidth * TexturePixelSize;
        textureData.SlicePitch = textureData.RowPitch * TextureHeight;

        UpdateSubresources(commandList, mTexture, textureUploadHeap, 0, 0, 1, &textureData);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(mTexture, &srvDesc, mSrvHeap->GetCPUDescriptorHandleForHeapStart());
    }
}

void TexturedTriangleRenderer::Render(ID3D12GraphicsCommandList* commandList)
{
    // Have to set the descriptor heap before setting the root signature
    ID3D12DescriptorHeap* heaps[] = { mSrvHeap };
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetGraphicsRootSignature(mRootSignature);
    commandList->SetPipelineState(mPipelineState);
    commandList->SetGraphicsRootDescriptorTable(0, mSrvHeap->GetGPUDescriptorHandleForHeapStart());
    commandList->RSSetViewports(1, &mViewport);
    commandList->RSSetScissorRects(1, &mScissorRect);

    // This is the actual stuff we are drawing
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    commandList->DrawInstanced(3, 1, 0, 0);
}

// ------------------------------------------------------------------------------------------------

class TextRenderer
{
public:
    TextRenderer() = default;
    ~TextRenderer();

    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, float screenWidth, float screenHeight);
    void RenderText(ID3D12GraphicsCommandList* commandList, std::string_view text, float x, float y, float scale = 1.0f);

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 uv;
    };

    static constexpr uint32_t MaxCharacters = 1024;

    CD3DX12_VIEWPORT mViewport;
    CD3DX12_RECT mScissorRect;

    ID3D12RootSignature* mRootSignature = nullptr;
    ID3D12PipelineState* mPipelineState = nullptr;
    ID3D12Resource* mVertexBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView = {};

    ID3D12DescriptorHeap* mSrvHeap = nullptr;
    ID3D12Resource* mFontTexture = nullptr;

    float mScreenWidth = 0;
    float mScreenHeight = 0;
}; 

TextRenderer::~TextRenderer()
{
    mRootSignature->Release();
    mPipelineState->Release();
    mVertexBuffer->Release();
    mSrvHeap->Release();
    mFontTexture->Release();
}

namespace Font
{
    const uint32_t TextureWidth = 256;
    const uint32_t TextureHeight = 256;
    const uint32_t TexturePixelSize = 4;  // RGBA
    const uint32_t CharWidth = 8;
    const uint32_t CharHeight = 16;
    const uint32_t CharsPerRow = TextureWidth / CharWidth;
    const uint32_t FirstChar = 0;  // Space
    const uint32_t NumChars = 128;   // Basic ASCII set

    // Dump of Sweet16mono.f8 from https://github.com/kmar/Sweet16Font
    constexpr std::array<std::array<uint8_t, 16>, 128> fontData = {{
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x28,0x28,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x24,0x24,0x7E,0x24,0x24,0x24,0x7E,0x24,0x24,0x00,0x00,0x00,0x00},
        {0x00,0x10,0x38,0x44,0x44,0x40,0x38,0x04,0x04,0x44,0x44,0x38,0x10,0x00,0x00,0x00},
        {0x00,0x00,0x40,0xA0,0xA2,0x44,0x08,0x10,0x20,0x44,0x8A,0x0A,0x04,0x00,0x00,0x00},
        {0x00,0x00,0x30,0x48,0x48,0x48,0x32,0x52,0x8C,0x84,0x8C,0x72,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x08,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x08,0x10,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x10,0x08,0x00,0x00,0x00},
        {0x00,0x00,0x20,0x10,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x10,0x20,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x24,0x18,0x7E,0x18,0x24,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x10,0x10,0x7C,0x10,0x10,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x10,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x04,0x04,0x08,0x08,0x10,0x10,0x20,0x20,0x40,0x40,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x46,0x4A,0x4A,0x52,0x52,0x62,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x04,0x0C,0x14,0x24,0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x02,0x04,0x08,0x10,0x20,0x40,0x40,0x7E,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x02,0x02,0x1C,0x02,0x02,0x02,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x04,0x0C,0x14,0x24,0x44,0x7E,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x7E,0x40,0x40,0x40,0x7C,0x02,0x02,0x02,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x1C,0x20,0x40,0x40,0x7C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x7E,0x02,0x02,0x02,0x04,0x08,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x42,0x42,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x42,0x42,0x3E,0x02,0x02,0x02,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x10,0x10,0x00,0x00,0x00,0x10,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x00,0x00,0x00,0x08,0x08,0x10,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x04,0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x04,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x40,0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x40,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x42,0x02,0x04,0x08,0x10,0x00,0x10,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x3C,0x42,0x99,0x85,0x9D,0xA5,0x9E,0x40,0x3E,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x7C,0x42,0x42,0x42,0x7C,0x42,0x42,0x42,0x42,0x7C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x40,0x40,0x40,0x40,0x40,0x40,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x78,0x44,0x42,0x42,0x42,0x42,0x42,0x42,0x44,0x78,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x7E,0x40,0x40,0x40,0x78,0x40,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x7E,0x40,0x40,0x40,0x78,0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x40,0x40,0x40,0x4E,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x42,0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x7C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x42,0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x82,0xC6,0xAA,0x92,0x92,0x82,0x82,0x82,0x82,0x82,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x7C,0x42,0x42,0x42,0x7C,0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x4A,0x46,0x3E,0x02,0x00,0x00,0x00},
        {0x00,0x00,0x7C,0x42,0x42,0x42,0x7C,0x44,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x3C,0x42,0x40,0x20,0x18,0x04,0x02,0x02,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0xFE,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x82,0x82,0x82,0x82,0x44,0x44,0x28,0x28,0x10,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x82,0x82,0x82,0x82,0x92,0x92,0x92,0xAA,0xC6,0x82,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x42,0x42,0x42,0x24,0x18,0x18,0x24,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x82,0x82,0x44,0x44,0x28,0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x7E,0x02,0x02,0x04,0x08,0x10,0x20,0x40,0x40,0x7E,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x38,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x38,0x00,0x00,0x00},
        {0x00,0x00,0x40,0x40,0x20,0x20,0x10,0x10,0x08,0x08,0x04,0x04,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x38,0x00,0x00,0x00},
        {0x00,0x10,0x28,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00},
        {0x00,0x00,0x10,0x10,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x3C,0x02,0x3E,0x42,0x42,0x42,0x3E,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x40,0x40,0x40,0x7C,0x42,0x42,0x42,0x42,0x42,0x7C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x02,0x02,0x02,0x3E,0x42,0x42,0x42,0x42,0x42,0x3E,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x3C,0x42,0x42,0x7E,0x40,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x1C,0x22,0x20,0x20,0x78,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x46,0x3A,0x02,0x42,0x3C,0x00},
        {0x00,0x00,0x40,0x40,0x40,0x7C,0x42,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x10,0x00,0x70,0x10,0x10,0x10,0x10,0x10,0x7C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x04,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x44,0x44,0x38,0x00},
        {0x00,0x00,0x40,0x40,0x40,0x42,0x42,0x44,0x78,0x44,0x42,0x42,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x70,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x7C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0xEC,0x92,0x92,0x92,0x92,0x92,0x82,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x7C,0x42,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x7C,0x42,0x42,0x42,0x42,0x42,0x7C,0x40,0x40,0x40,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x3E,0x42,0x42,0x42,0x42,0x42,0x3E,0x02,0x02,0x02,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x5C,0x60,0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x3C,0x42,0x40,0x3C,0x02,0x42,0x3C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x10,0x10,0x7C,0x10,0x10,0x10,0x10,0x10,0x0C,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3E,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x82,0x82,0x44,0x44,0x28,0x28,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x82,0x82,0x92,0x92,0x92,0xAA,0x44,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x82,0x44,0x28,0x10,0x28,0x44,0x82,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x46,0x3A,0x02,0x04,0x78,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x7E,0x04,0x08,0x10,0x20,0x40,0x7E,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x0C,0x10,0x10,0x10,0x10,0x60,0x10,0x10,0x10,0x10,0x0C,0x00,0x00,0x00},
        {0x00,0x00,0x10,0x10,0x10,0x10,0x00,0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x60,0x10,0x10,0x10,0x10,0x0C,0x10,0x10,0x10,0x10,0x60,0x00,0x00,0x00},
        {0x00,0x00,0x32,0x4C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
    }};

    std::vector<uint8_t> GenerateTextureData()
    {
        std::vector<uint8_t> data(TextureWidth * TextureHeight * TexturePixelSize, 0);

        // For each character in our font
        for (uint32_t charIndex = 0; charIndex < NumChars; ++charIndex)
        {
            char c = static_cast<char>(FirstChar + charIndex);
            uint32_t gridX = (charIndex % CharsPerRow);
            uint32_t gridY = (charIndex / CharsPerRow);

            // For each pixel in the character
            for (uint32_t y = 0; y < CharHeight; ++y)
            {
                for (uint32_t x = 0; x < CharWidth; ++x)
                {
                    // Get the pixel from our font data
                    bool isSet = (fontData[charIndex][y] & (1 << (CharWidth - 1 - x))) != 0;
                    
                    // Calculate position in our texture
                    uint32_t texX = gridX * CharWidth + x;
                    uint32_t texY = gridY * CharHeight + y;
                    uint32_t texIndex = (texY * TextureWidth + texX) * TexturePixelSize;

                    // Set the pixel color (white if set, transparent if not)
                    data[texIndex + 0] = isSet ? 0xFF : 0x00;  // R
                    data[texIndex + 1] = isSet ? 0xFF : 0x00;  // G
                    data[texIndex + 2] = isSet ? 0xFF : 0x00;  // B
                    data[texIndex + 3] = isSet ? 0xFF : 0x00;  // A
                }
            }
        }

        return data;
    }
}

void TextRenderer::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, float screenWidth, float screenHeight)
{
    mScreenWidth = screenWidth;
    mScreenHeight = screenHeight;

    mViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, screenWidth, screenHeight);
    mScissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(screenWidth), static_cast<LONG>(screenHeight));

    // Create descriptor heap for font texture
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ensure(SUCCEEDED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap))));

    // Create root signature
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[1];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

        CD3DX12_ROOT_PARAMETER rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(1, rootParameters, 1, &sampler, 
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ID3DBlob* signature;
        ID3DBlob* error;
        ensure(SUCCEEDED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)));
        ensure(SUCCEEDED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature))));
        signature->Release();
        if (error) error->Release();
    }

    // Create pipeline state
    {
        ID3DBlob* vertexShader;
        ID3DBlob* pixelShader;

#if defined(_DEBUG)
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ensure(SUCCEEDED(D3DCompileFromFile(L"data/textured.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr)));
        ensure(SUCCEEDED(D3DCompileFromFile(L"data/textured.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr)));

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

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

        // Enable alpha blending
        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        ensure(SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState))));
    }

    // Create vertex buffer
    {
        const uint32_t vertexBufferSize = MaxCharacters * 6 * sizeof(Vertex); // 6 vertices per quad
        ensure(SUCCEEDED(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mVertexBuffer))));

        mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
        mVertexBufferView.StrideInBytes = sizeof(Vertex);
        mVertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Create font texture
    {
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = Font::TextureWidth;
        textureDesc.Height = Font::TextureHeight;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ensure(SUCCEEDED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                         D3D12_HEAP_FLAG_NONE,
                                                         &textureDesc,
                                                         D3D12_RESOURCE_STATE_COPY_DEST,
                                                         nullptr,
                                                         IID_PPV_ARGS(&mFontTexture))));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mFontTexture, 0, 1);
        ID3D12Resource* textureUploadHeap;
        ensure(SUCCEEDED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                         D3D12_HEAP_FLAG_NONE,
                                                         &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                                                         D3D12_RESOURCE_STATE_GENERIC_READ,
                                                         nullptr,
                                                         IID_PPV_ARGS(&textureUploadHeap))));

        std::vector<uint8_t> textureData = Font::GenerateTextureData();
        D3D12_SUBRESOURCE_DATA textureSubresourceData = {};
        textureSubresourceData.pData = textureData.data();
        textureSubresourceData.RowPitch = Font::TextureWidth * Font::TexturePixelSize;
        textureSubresourceData.SlicePitch = textureSubresourceData.RowPitch * Font::TextureHeight;

        UpdateSubresources(commandList, mFontTexture, textureUploadHeap, 0, 0, 1, &textureSubresourceData);
        commandList->ResourceBarrier(1,
                                     &CD3DX12_RESOURCE_BARRIER::Transition(mFontTexture, 
                                                                           D3D12_RESOURCE_STATE_COPY_DEST,
                                                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(mFontTexture, &srvDesc, mSrvHeap->GetCPUDescriptorHandleForHeapStart());
    }
}

namespace Font
{
    void GetCharacterUVs(char c, float& u1, float& v1, float& u2, float& v2)
    {
        uint32_t charIndex = static_cast<uint32_t>(c) - FirstChar;
        uint32_t gridX = charIndex % CharsPerRow;
        uint32_t gridY = charIndex / CharsPerRow;

        u1 = static_cast<float>(gridX * CharWidth) / TextureWidth;
        v1 = static_cast<float>(gridY * CharHeight) / TextureHeight;
        u2 = static_cast<float>((gridX + 1) * CharWidth) / TextureWidth;
        v2 = static_cast<float>((gridY + 1) * CharHeight) / TextureHeight;
    }
}

void TextRenderer::RenderText(ID3D12GraphicsCommandList* commandList, std::string_view text, float x, float y, float scale)
{
    // Map vertex buffer
    Vertex* vertices;
    CD3DX12_RANGE readRange(0, 0);
    ensure(SUCCEEDED(mVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertices))));

    const float charWidth = (Font::CharWidth * scale) / mScreenWidth * 2.0f;
    const float charHeight = (Font::CharHeight * scale) / mScreenHeight * 2.0f;
    float currentX = (x / mScreenWidth * 2.0f) - 1.0f;
    float currentY = 1.0f - (y / mScreenHeight * 2.0f);

    uint32_t vertexCount = 0;
    for (char c : text)
    {
        if (vertexCount + 6 > MaxCharacters * 6) break;

        float u1, v1, u2, v2;
        Font::GetCharacterUVs(c, u1, v1, u2, v2);

        // First triangle
        vertices[vertexCount++] = { { currentX, currentY, 0.0f }, { u1, v1 } };
        vertices[vertexCount++] = { { currentX + charWidth, currentY - charHeight, 0.0f }, { u2, v2 } };
        vertices[vertexCount++] = { { currentX, currentY - charHeight, 0.0f }, { u1, v2 } };

        // Second triangle
        vertices[vertexCount++] = { { currentX, currentY, 0.0f }, { u1, v1 } };
        vertices[vertexCount++] = { { currentX + charWidth, currentY, 0.0f }, { u2, v1 } };
        vertices[vertexCount++] = { { currentX + charWidth, currentY - charHeight, 0.0f }, { u2, v2 } };

        currentX += charWidth;
    }

    mVertexBuffer->Unmap(0, nullptr);

    ID3D12DescriptorHeap* heaps[] = { mSrvHeap };
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetGraphicsRootSignature(mRootSignature);
    commandList->SetPipelineState(mPipelineState);
    commandList->SetGraphicsRootDescriptorTable(0, mSrvHeap->GetGPUDescriptorHandleForHeapStart());

    commandList->RSSetViewports(1, &mViewport);
    commandList->RSSetScissorRects(1, &mScissorRect);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    commandList->DrawInstanced(vertexCount, 1, 0, 0);
} 

// ------------------------------------------------------------------------------------------------

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

    TexturedTriangleRenderer mTriangleRenderer;
    TextRenderer mTextRenderer;

    uint32_t mFrameIndex;
    ID3D12Fence* mFence;
    HANDLE mFenceEvent;
    uint64_t mFenceValue;
};

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
    IDXGIAdapter1* adapter = nullptr;
    ensure(SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory))));
    // Ignore this line. I'm testing in a VM and don't have a GPU so I'm using the warp adapter which is basically a software renderer.
    // ensure(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
    ensure(SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice))));
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
    ensure(SUCCEEDED(mCommandList->Reset(mCommandAllocator, nullptr)));
 
    mTriangleRenderer.Initialize(mDevice, mCommandList, static_cast<float>(mWidth), static_cast<float>(mHeight));
    mTextRenderer.Initialize(mDevice, mCommandList, static_cast<float>(mWidth), static_cast<float>(mHeight));

    // Close the command list and execute it to begin the initial GPU setup.
    ensure(SUCCEEDED(mCommandList->Close()));
    ID3D12CommandList* commandLists[] = { mCommandList };
    mCommandQueue->ExecuteCommandLists(1, commandLists);
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

    mTriangleRenderer.Render(mCommandList);
    mTextRenderer.RenderText(mCommandList, "Hello World!", 100.0f, 100.0f, 1.0f);
    // Transition back buffer back to the present state since we are done drawing to it and want it ready for present
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ensure(SUCCEEDED(mCommandList->Close()));

    ID3D12CommandList* commandLists[] = { mCommandList };
    mCommandQueue->ExecuteCommandLists(1, commandLists);
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

// ------------------------------------------------------------------------------------------------

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
