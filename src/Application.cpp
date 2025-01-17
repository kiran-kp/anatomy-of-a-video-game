#include <Application.h>
#include <Log.h>

#include <thread>

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
    auto log_thread = std::thread([]() {
        using namespace std::chrono_literals;
        while (true)
        {
            LOGGER_FLUSH();
            std::this_thread::sleep_for(10ms);
        }
    });

    log_thread.detach();

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