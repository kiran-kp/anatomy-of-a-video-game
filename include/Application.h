#pragma once

#include <Audio.h>
#include <Renderer.h>
#include <SpriteInstance.h>
#include <Window.h>

#include <array>
#include <memory>
#include <random>


class Application
{
public:
    ~Application();

    // Be explicit about the initialization and destruction of singletons so that their lifetimes are known.
    // We don't care about shutting down the Application class because it is only supposed to get destroyed when the program exits.
    static void Initialize(HINSTANCE hInstance, int nCmdShow);
    static Application& Instance();

    void Run();

    // Events that are triggered from the Windows message loop
    void KeyDown();
    void KeyUp();

private:
    // The constructors are private/deleted to prevent instantiation of the singleton outside of the Initialize function.
    Application();
    Application(const Application&) = delete;

    void Update(float deltaTime);
    void Render();

    void AddDebugText(std::string_view, int x, int y);
    void AddText(std::string_view, int x, int y);

    Window mWindow;
    Renderer mRenderer;
    Audio mAudio;

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


    static std::unique_ptr<Application> mInstance;
};
