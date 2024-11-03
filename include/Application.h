#pragma once

#include <memory>

class Application
{
public:
    ~Application();

    // Be explicit about the initialization and destruction of singletons so that their lifetimes are known.
    // We don't care about shutting down the Application class because it is only supposed to get destroyed when the program exits.
    static void Initialize();
    static Application& Instance();

    // Events that are triggered from the Windows message loop
    void Update();
    void Render();
    void KeyDown(uint8_t key);
    void KeyUp(uint8_t key);

private:
    // The constructors are private/deleted to prevent instantiation of the singleton outside of the Initialize function.
    Application();
    Application(const Application&) = delete;

    static std::unique_ptr<Application> mInstance;
};
