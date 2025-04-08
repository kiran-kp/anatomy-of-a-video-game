#pragma once

#include <Audio.h>
#include <Common.h>
#include <Renderer.h>
#include <SpriteInstance.h>

#include <array>
#include <memory>
#include <random>

class Application
{
public:
    Application(Arena* arena, void* platformData);
    ~Application();

    static Application* Instance();

    void Update(float deltaTime);
    void Render();

    // Events that are triggered from the Windows message loop
    void KeyDown();
    void KeyUp();

private:
    Application() = delete;
    Application(const Application&) = delete;

    void AddDebugText(std::string_view, int x, int y);
    void AddText(std::string_view, int x, int y);

    Arena* mArena;

    Renderer mRenderer;
    Audio* mAudio;

    Audio::Ref mDie;
    Audio::Ref mHit;
    Audio::Ref mPoint;
    Audio::Ref mSwoosh;
    Audio::Ref mWing;

    Sprite mBackground;
    Sprite mBird;
    Sprite mPipe;
    Sprite mBase;
    Sprite mGameOver;

    static constexpr float Gravity = 0.0025f;

    float mScrollSpeed;
    float mBirdYVelocity;
    bool mKeydown;
    float mDeadTimer;
    bool mPlaying;
    float mScore;
    float mHiScore;
    SpriteInstance mBirdInstance;

    std::random_device mRandomDevice;
    std::mt19937 mRng;
    std::array<SpriteInstance, 6> mPipeInstances;
    std::array<SpriteInstance, 2> mBaseInstances;

    static Application* sInstance;
};
