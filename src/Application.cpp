#include <Application.h>

std::unique_ptr<Application> Application::mInstance;

Application::Application()
{
}

Application::~Application()
{
}

void Application::Initialize()
{
    mInstance.reset(new Application());
}

Application& Application::Instance()
{
    return *mInstance;
}

void Application::Update()
{
}

void Application::Render()
{
}

void Application::KeyDown(uint8_t key)
{
}

void Application::KeyUp(uint8_t key)
{
}