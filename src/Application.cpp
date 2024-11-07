#include <Application.h>

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
    mInstance->mRenderer.Initialize(mInstance->mWindow);
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