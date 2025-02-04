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
    void FinishUploadingTextures();
    void Shutdown();

    void Render();

    void AddDebugText(std::string_view text, int32_t x, int32_t y);
    
    TextureRef CreateTexture(std::string_view path);
    void AddQuad(float x, float y, float width, float height, bool flipX, bool flipY, TextureRef texture);

private:
    std::unique_ptr<RendererImpl> mImpl;
};