#pragma once

#include <memory>
#include <string_view>

class Window;
class RendererImpl;

struct TextureRef
{
    size_t index;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Initialize(Window& window);
    void Shutdown();

    void Render();

    void AddDebugText(std::string_view text, int32_t x, int32_t y);

    TextureRef CreateTexture(uint32_t width, uint32_t height, uint32_t pixelSize, const void* data);

private:
    std::unique_ptr<RendererImpl> mImpl;
};