#pragma once

#include <memory>
#include <string_view>

class Window;
class RendererImpl;

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Initialize(Window& window);
    void Shutdown();

    void Render();

    void AddDebugText(std::string_view text, int32_t x, int32_t y);

private:
    std::unique_ptr<RendererImpl> mImpl;
};