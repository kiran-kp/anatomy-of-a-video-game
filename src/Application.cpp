#include <Application.h>
#include <Log.h>
#include <Util.h>

#include <cassert>
#include <chrono>
#include <format>
#include <string_view>
#include <thread>
#include <vector>
#include <unordered_map>

std::unique_ptr<Application> Application::mInstance;

struct Image
{
    std::string path;
    float width;
    float height;
};

Application::Application()
    : mWindow()
    , mRenderer()
    , mBackground()
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

    std::vector<std::pair<Sprite*, Image>> images = {
        { &mInstance->mBird, { "assets/sprites/bluebird-downflap.png", 34.0f, 24.0f } },
        { &mInstance->mBird, { "assets/sprites/bluebird-midflap.png", 34.0f, 24.0f } },
        { &mInstance->mBird, { "assets/sprites/bluebird-upflap.png", 34.0f, 24.0f } },
        { &mInstance->mBackground, { "assets/sprites/background-night.png", 288.0f, 512.0f } },
        { &mInstance->mPipe, { "assets/sprites/pipe-green.png", 52.0f, 320.0f } },
        { &mInstance->mBase, { "assets/sprites/base.png", 336.0f, 112.0f } },
        { &mInstance->mGameOver, { "assets/sprites/gameover.png", 192.0f, 42.0f } }
    };

    for (auto& [sprite, image] : images)
    {
        sprite->Initialize(image.width, image.height, 160.0f);
        sprite->AddTexture(mInstance->mRenderer.CreateTexture(image.path));
    }

	mInstance->mBirdInstance.Initialize(&mInstance->mBird);

    LOG("Initialized Textures");
    mInstance->mRenderer.FinishUploadingTextures();
    LOG("Uploaded textures to GPU");
}

Application& Application::Instance()
{
    return *mInstance;
}

void Application::Run()
{
    std::chrono::high_resolution_clock::time_point lastFrameTime(std::chrono::high_resolution_clock::now());

    while (mWindow.ProcessMessages())
    {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto deltaTime = std::chrono::duration<float, std::milli>(now - lastFrameTime).count();
        lastFrameTime = now;

        Update(deltaTime);
        Render();
    }
}

static float x = 50.0f;
static float y = 50.0f;

static float xDir = 1.0f;
static float yDir = 1.0f;

void Application::Update(float deltaTime)
{
    mBirdInstance.Update(deltaTime);

    x += 100.0f * xDir * deltaTime / 1000.0f;
    y += 100.0f * yDir * deltaTime / 1000.0f;

    if ((x + mBird.GetWidth()) >= 288.0f || x <= 0.0f)
    {
        xDir *= -1.0f;
    }

    if ((y + mBird.GetHeight()) >= 512.0f || y <= 0.0f)
    {
        yDir *= -1.0f;
    }
	mBirdInstance.SetPosition(x, y);
	mBirdInstance.SetFlip(xDir < 0.0f, false);
    mRenderer.AddDebugText(std::format("Frame time: {:.4}", deltaTime), 100, 100);
    mRenderer.AddQuad(95.0f, 95.0f, 150.0f, 20.0f, DarkBlue);
}

void Application::Render()
{
    mBackground.RenderFrame(mRenderer, 0.0f, 0.0f, 0);
    mBirdInstance.Render(mRenderer);

    mRenderer.Render();
}

void Application::KeyDown()
{
}

void Application::KeyUp()
{
}