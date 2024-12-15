#include <Application.h>
#include <Log.h>
std::unique_ptr<Application> Application::mInstance;

Application::Application()
    : mWindow()
    , mRenderer()
{
}

Application::~Application()
{
}

void Application::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    mInstance.reset(new Application());
    mInstance->mWindow.Initialize(L"Bird Game", 288, 512, hInstance, nCmdShow);
    LOG("Initialized Window");
    mInstance->mRenderer.Initialize(mInstance->mWindow);
    LOG("Initialized Renderer");
}

Application& Application::Instance()
{
    return *mInstance;
}

void Application::Run()
{
    while (mWindow.ProcessMessages())
    {
        Update();
        Render();
    }
}

void Application::Update()
{
    LOGGER_FLUSH();
    mRenderer.AddDebugText("Hello World!", 100, 100);
}

void Application::Render()
{
    mRenderer.Render();
}

void Application::KeyDown()
{
}

void Application::KeyUp()
{
}