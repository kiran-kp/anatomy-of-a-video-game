#pragma once

#include <memory>

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

private:
    std::unique_ptr<RendererImpl> mImpl;
};