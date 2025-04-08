#include <Application.h>
#include <Log.h>
#include <Util.h>

#include <cassert>
#include <chrono>
#include <format>
#include <string_view>

constexpr float BasePos = 512.0f - 112.0f;
constexpr float GameScrollSpeed = 0.1f;

Application* Application::sInstance = nullptr;

static float Clamp(float x, float minVal, float maxVal)
{
    return std::max(std::min(x, maxVal), minVal);
}

struct Image
{
    float width;
    float height;
    std::initializer_list<std::string_view> paths;
};

static void ResetPipes(std::array<SpriteInstance, 6>& pipes, Sprite* pipeSprite, const uint32_t windowHeight)
{
    size_t i = 6;
    static std::random_device rd;
    static std::mt19937 rng(rd());
    bool shouldCombine = false;
    for (auto& p : pipes)
    {
        // 0 - top, 1 - bottom, 2 - both
        std::uniform_int_distribution pipePosSelector(0, 2);
        auto config = pipePosSelector(rng);

        p.Initialize(pipeSprite);
        Vec2 pos = { i * pipeSprite->GetWidth(), -200.0f };
        bool flipped = true;
        if (shouldCombine)
        {
            shouldCombine = false;
            pos.x -= pipeSprite->GetWidth();
            pos.y = (100.0f + windowHeight - pipeSprite->GetHeight());
            flipped = false;
        }
        else if (config == 1)
        {
            pos.y = (100.0f + windowHeight - pipeSprite->GetHeight());
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
}

Application::Application(Arena* arena, void* platformData)
    : mArena(arena)
    , mRenderer()
    , mAudio()
    , mDie()
    , mHit()
    , mPoint()
    , mSwoosh()
    , mWing()
    , mBackground()
    , mBird()
    , mPipe()
    , mBase()
    , mGameOver()
    , mScrollSpeed(GameScrollSpeed)
    , mBirdYVelocity()
    , mKeydown(false)
    , mDeadTimer(0.0f)
    , mPlaying(false)
    , mScore(0.0f)
    , mHiScore(0.0f)
    , mBirdInstance()
    , mRandomDevice()
    , mRng(mRandomDevice())
    , mPipeInstances()
    , mBaseInstances()

{
    sInstance = this;

    mRenderer.Initialize(platformData);
    LOG("Initialized Renderer");

    mAudio = mArena->Push<Audio>();
    auto audioArena = mArena->PushArena("ARENA_Audio", 1ll * 1024ll * 1024ll);
    mAudio->Initialize(audioArena);
    LOG("Initialized Audio");

    std::pair<Audio::Ref*, std::string_view> sounds[] = {
        { &mDie, "assets/audio/die.wav" },
        { &mHit, "assets/audio/hit.wav" },
        { &mPoint, "assets/audio/point.wav" },
        { &mSwoosh, "assets/audio/swoosh.wav" },
        { &mWing, "assets/audio/wing.wav" },
    };

    for (auto& [ref, path] : sounds)
    {
        *ref = mAudio->LoadSound(path);
    }

    std::pair<Sprite*, Image> images[] = {
        { &mBird, { 34.0f, 24.0f, { "assets/sprites/bluebird-downflap.png", "assets/sprites/bluebird-midflap.png", "assets/sprites/bluebird-upflap.png" } } },
        { &mBackground, { 288.0f, 512.0f, { "assets/sprites/background-night.png" } } },
        { &mPipe, { 52.0f, 320.0f, { "assets/sprites/pipe-green.png" } } },
        { &mBase, { 336.0f, 112.0f, { "assets/sprites/base.png" } } },
        { &mGameOver, { 192.0f, 42.0f, { "assets/sprites/gameover.png" } } }
    };

    for (auto& [sprite, image] : images)
    {
        auto textures = mArena->PushArray<TextureRef>(image.paths.size());
        for (size_t i = 0; auto& path : image.paths)
        {
            textures[i] = mRenderer.CreateTexture(path);
            i++;
        }

        sprite->Initialize(image.width, image.height, textures, 160.0f);
    }

    mBirdInstance.Initialize(&mBird);
    mBirdInstance.SetPosition({ 50.0f, 200.0f });
    mBirdYVelocity = 0.0f;

    ResetPipes(mPipeInstances, &mPipe, WindowHeight);

    size_t i = 0;
    for (auto& b : mBaseInstances)
    {
        b.Initialize(&mBase);
        b.SetPosition({ i * mBase.GetWidth() + i, BasePos});
        i++;
    }

    LOG("Initialized Textures");
    mRenderer.FinishUploadingTextures();
    LOG("Uploaded textures to GPU");
}

Application::~Application()
{
}

void Application::Update(float deltaTime)
{
    bool isAlive = mDeadTimer <= 0.0f;
    // Update base
    if (isAlive)
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
    if (mPlaying)
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

    // Update player
    {
        Vec2 pos = mBirdInstance.GetPosition();
        if (isAlive)
        {
            if (mKeydown)
            {
                mBirdYVelocity = -0.35f;
                mAudio->Play(mWing);
                mPlaying = true;
            }
        }
        else
        {
            mDeadTimer -= deltaTime;
            if (mDeadTimer <= 0.0f)
            {
                pos = { 50.0f, 200.0f };
                mPlaying = false;
                isAlive = true;
                ResetPipes(mPipeInstances, &mPipe, WindowHeight);
            }
        }

        bool collided = false;
        auto updateCollided = [&collided](bool hasCollided) { collided = collided || hasCollided; };

        {
            if (mPlaying || !isAlive)
            {
                pos.y += deltaTime * (mBirdYVelocity + (deltaTime * Gravity / 2));
                mBirdYVelocity += deltaTime * Gravity;
                float unclampedY = pos.y;
                pos.y = Clamp(pos.y, 0.0f, BasePos - mBird.GetHeight());
                //updateCollided(unclampedY != pos.y);
            }

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
                        break;
                    }
                }
            }

            if (mPlaying && collided)
            {
                mHiScore = std::max(mScore, mHiScore);
                mScore = 0.0f;
                mAudio->Play(mDie);
                mDeadTimer = 5000.0f;
                mPlaying = false;
            }
        }

        mBirdInstance.SetPosition(pos);
        if (isAlive)
        {
            mBirdInstance.Update(deltaTime);
        }

        AddDebugText(std::format("({},{})", static_cast<int>(pos.x), static_cast<int>(pos.y)), static_cast<int>(pos.x), static_cast<int>(pos.y) - 16);
    }

    if (mPlaying)
    {
        float oldScore = mScore;
        mScore += deltaTime / 1000.0f;
        if (oldScore < mHiScore && mScore > mHiScore)
        {
            mAudio->Play(mSwoosh);
        }
        else if (mScore > 1.0f && (static_cast<int>(mScore) % 10) == 0)
        {
            mAudio->Play(mPoint);
        }
    }

    AddText(std::format("Frame time: {:.4}", deltaTime), 0, 0);
    AddText(std::format("Hi Score: {:.2f}", mHiScore), 0, 16);
    AddText(std::format("Score: {:.2f}", mScore), 0, 32);
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
#if 1
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
