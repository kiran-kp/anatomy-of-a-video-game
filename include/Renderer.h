#pragma once

#include <memory>
#include <string_view>

class Window;
class RendererImpl;

struct TextureRef
{
    size_t index;
};

struct Color
{
    float r;
    float g;
    float b;
    float a;
};

constexpr Color White = { 1.0f, 1.0f, 1.0f, 1.0f };
constexpr Color Black = { 0.0f, 0.0f, 0.0f, 1.0f };
constexpr Color LightGray = { 0.75f, 0.75f, 0.75f, 1.0f };
constexpr Color DarkGray = { 0.25f, 0.25f, 0.25f, 1.0f };
constexpr Color DarkBlue = { 0.0f, 0.2f, 0.5f, 1.0f };

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Initialize(Window& window);
    void FinishUploadingTextures();
    void Shutdown();

    void Render();

    void AddDebugText(const std::string_view text, const int32_t x, const int32_t y);
    
    TextureRef CreateTexture(std::string_view path);
    void AddQuad(const float x,
                 const float y,
                 const float width,
                 const float height,
                 const bool flipX,
                 const bool flipY,
                 const TextureRef texture);
    void AddQuad(const float x,
                 const float y,
                 const float width,
                 const float height,
                 const Color& color);

private:
    std::unique_ptr<RendererImpl> mImpl;
};