#pragma once

#include <Renderer.h>
#include <Window.h>

#include <memory>

class Application
{
public:
    ~Application();

    // Be explicit about the initialization and destruction of singletons so that their lifetimes are known.
    // We don't care about shutting down the Application class because it is only supposed to get destroyed when the program exits.
    static void Initialize(HINSTANCE hInstance, int nCmdShow);
    static Application& Instance();

    void Run();

    // Events that are triggered from the Windows message loop
    void KeyDown();
    void KeyUp();

private:
    // The constructors are private/deleted to prevent instantiation of the singleton outside of the Initialize function.
    Application();
    Application(const Application&) = delete;

    void Update();
    void Render();

    Window mWindow;
    Renderer mRenderer;

    static std::unique_ptr<Application> mInstance;
};
