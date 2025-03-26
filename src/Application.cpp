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

constexpr float BasePos = 512.0f - 112.0f;

float Clamp(float x, float minVal, float maxVal)
{
    return max(min(x, maxVal), minVal);
}

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
    mInstance->mBirdInstance.SetPosition({50.0f, 200.0f});
    mInstance->mBirdYVelocity = 0.0f;
    int i = 0;
    bool flipped = false;
    for (auto& p : mInstance->mPipeInstances)
    {
        p.Initialize(&mInstance->mPipe);
        p.SetPosition({ i * mInstance->mPipe.GetWidth(), 20.0f });
        p.SetFlip(false, flipped);
        flipped = !flipped;
        i++;
    }

    i = 0;
    for (auto& b : mInstance->mBaseInstances)
    {
        b.Initialize(&mInstance->mBase);
        b.SetPosition({ i * mInstance->mBase.GetWidth(), BasePos });
        i++;
    }

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

void Application::Update(float deltaTime)
{
    bool collided = false;
    auto updateCollided = [&collided](bool hasCollided) { collided = collided || hasCollided; };

    // Update player
    {
        Vec2 pos = mBirdInstance.GetPosition();
        pos.y += deltaTime * (mBirdYVelocity + (deltaTime * Gravity / 2));
        mBirdYVelocity += deltaTime * Gravity;
        float unclampedY = pos.y;
        pos.y = Clamp(pos.y, 0.0f, BasePos - mBird.GetHeight());
        updateCollided(unclampedY != pos.y);

        mBirdInstance.SetPosition(pos);
        mBirdInstance.Update(deltaTime);
    }

    // Update base
    {
        for (auto& b : mBaseInstances)
        {
            Vec2 pos = b.GetPosition();
            pos.x -= 0.1f * deltaTime;
            if (pos.x <= -mBase.GetWidth())
            {
                pos.x = mBase.GetWidth();
            }

            b.SetPosition(pos);
        }
    }

    mRenderer.AddDebugText(std::format("Frame time: {:.4}", deltaTime), 0, 0);
    mRenderer.AddQuad(0.0f, 0.0f, 150.0f, 20.0f, DarkBlue);
}

void Application::Render()
{
    mBackground.RenderFrame(mRenderer, 0.0f, 0.0f, 0);
    mBase.RenderFrame(mRenderer, 0.0f, BasePos, 0);

    for (auto& p : mPipeInstances)
    {
        p.Render(mRenderer);
    }

    for (auto& b : mBaseInstances)
    {
        b.Render(mRenderer);
    }

    mBirdInstance.Render(mRenderer);

    mRenderer.Render();
}

void Application::KeyDown()
{
}

void Application::KeyUp()
{
}