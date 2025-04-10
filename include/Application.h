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
    void Initialize(Arena* arena, void* platformData);
    void Update(float deltaTime);
    void Render();

    // Events that are triggered from the Windows message loop
    void KeyDown();
    void KeyUp();

    void AddDebugText(std::string_view, int x, int y);

private:
    Application() = delete;
    ~Application() = delete;
    Application(const Application&) = delete;

    void AddText(std::string_view, int x, int y);

    Arena* mArena;

    Renderer* mRenderer;
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
