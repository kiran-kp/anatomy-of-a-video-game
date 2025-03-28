#include <Application.h>
#include <Log.h>
#include <Util.h>

#include <cassert>
#include <chrono>
#include <format>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

constexpr float BasePos = 512.0f - 112.0f;

static float Clamp(float x, float minVal, float maxVal)
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
    , mBird()
    , mPipe()
    , mBase()
    , mGameOver()
    , mBirdYVelocity()
    , mKeydown(false)
    , mScore(0.0f)
    , mHiScore(0.0f)
    , mBirdInstance()
    , mRandomDevice()
    , mRng(mRandomDevice())
    , mPipeInstances()
    , mBaseInstances()
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
    std::random_device rd;
    std::mt19937 rng(rd());
    bool shouldCombine = false;
    for (auto& p : mInstance->mPipeInstances)
    {
        // 0 - top, 1 - bottom, 2 - both
        std::uniform_int_distribution pipePosSelector(0, 2);
        auto config = pipePosSelector(rng);

        p.Initialize(&mInstance->mPipe);
        Vec2 pos = { i * mInstance->mPipe.GetWidth(), -200.0f };
        bool flipped = true;
        if (shouldCombine)
        {
            shouldCombine = false;
            pos.x -= mInstance->mPipe.GetWidth();
            pos.y = (100.0f + mInstance->mWindow.GetHeight() - mInstance->mPipe.GetHeight());
            flipped = false;
        }
        else if (config == 1)
        {
            pos.y = (100.0f + mInstance->mWindow.GetHeight() - mInstance->mPipe.GetHeight());
            flipped = false;
        }
        else if (config == 2)
        {
            shouldCombine = true;
        }

        p.SetPosition(pos);
        p.SetFlip(false, flipped);
        flipped = !flipped;
        i += 4;
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
    // Update base
    {
        for (auto& b : mBaseInstances)
        {
            Vec2 pos = b.GetPosition();
            pos.x -= mScrollSpeed * deltaTime;
            if (pos.x <= -mBase.GetWidth())
            {
                pos.x = mBase.GetWidth();
            }

            b.SetPosition(pos);
        }
    }

    // Update pipes
    {
        for (auto& p : mPipeInstances)
        {
            Vec2 pos = p.GetPosition();
            pos.x -= mScrollSpeed * deltaTime;
            if (pos.x <= -mPipe.GetWidth())
            {
                pos.x = mPipeInstances.size() * mPipe.GetWidth() * 2;

                std::uniform_real_distribution yDist(-50.0f, 50.0f);
                pos.y += yDist(mRng);
                pos.y = Clamp(pos.y, -(mPipe.GetHeight() / 2.0f), BasePos);
            }

            p.SetPosition(pos);
            if (p.GetYFlipped())
            {
                AddDebugText(std::format("({},{})", static_cast<int>(pos.x), static_cast<int>(pos.y)), static_cast<int>(pos.x), static_cast<int>(pos.y) + static_cast<int>(mPipe.GetHeight()));
            }
            else
            {
                AddDebugText(std::format("({},{})", static_cast<int>(pos.x), static_cast<int>(pos.y)), static_cast<int>(pos.x), static_cast<int>(pos.y) - 16);
            }
        }
    }

    bool collided = false;
    auto updateCollided = [&collided](bool hasCollided) { collided = collided || hasCollided; };

    // Update player
    {
        Vec2 pos = mBirdInstance.GetPosition();
        if (mKeydown)
        {
            mBirdYVelocity = -0.35f;
        }

        pos.y += deltaTime * (mBirdYVelocity + (deltaTime * Gravity / 2));
        mBirdYVelocity += deltaTime * Gravity;
        float unclampedY = pos.y;
        pos.y = Clamp(pos.y, 0.0f, BasePos - mBird.GetHeight());
        //updateCollided(unclampedY != pos.y);

        for (const auto& p : mPipeInstances)
        {
            auto pipePos = p.GetPosition();
            if (pipePos.x > (50.0f - mPipe.GetWidth()))
            {
                auto top = pipePos.y;
                auto bottom = pipePos.y + mPipe.GetHeight();
                if ((pipePos.x <= pos.x + mBird.GetWidth()) && (pos.y > top) && (pos.y < bottom))
                {
                    collided = true;
                    mHiScore = max(mScore, mHiScore);
                    mScore = 0.0f;
                }
            }
        }

        mBirdInstance.SetPosition(pos);
        mBirdInstance.Update(deltaTime);

        AddDebugText(std::format("({},{})", static_cast<int>(pos.x), static_cast<int>(pos.y)), static_cast<int>(pos.x), static_cast<int>(pos.y) - 16);
    }

    mScore += deltaTime / 1000.0f;
    AddText(std::format("Frame time: {:.4}", deltaTime), 0, 0);
    AddText(std::format("Hi Score: {:.2}", mHiScore), 0, 16);
    AddText(std::format("Score: {}", mScore), 0, 32);
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

void Application::AddDebugText(std::string_view txt, int x, int y)
{
#if 0
    mRenderer.AddDebugText(txt, x, y);
    mRenderer.AddQuad(static_cast<float>(x), static_cast<float>(y), static_cast<float>(txt.size() * 8), 16.0f, DarkBlue);
#endif
}

void Application::AddText(std::string_view txt, int x, int y)
{
    mRenderer.AddDebugText(txt, x, y);
    mRenderer.AddQuad(static_cast<float>(x), static_cast<float>(y), static_cast<float>(txt.size() * 8), 16.0f, DarkBlue);
}

void Application::KeyDown()
{
    mKeydown = true;
}

void Application::KeyUp()
{
    mKeydown = false;
}