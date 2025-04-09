#include <Application.h>
#include <Common.h>
#include <Log.h>
#include <Util.h>

#include <thread>

#include <Windows.h>

Application* appInstance = nullptr;

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        appInstance->KeyDown();
        return 0;
    }
    case WM_LBUTTONUP:
    {
        appInstance->KeyUp();
        return 0;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
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

    LOG("Initialized Logger");

    // Initialize the window
    const wchar_t CLASS_NAME[] = L"BirdGame";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    auto hwnd = CreateWindowEx(0,
                           CLASS_NAME,
                           L"BirdGame",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           WindowWidth, WindowHeight,
                           NULL,
                           NULL,
                           hInstance,
                           NULL);

    ensure(hwnd != NULL);

    ShowWindow(hwnd, nCmdShow);

    SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) & ~WS_SIZEBOX);

    LOG("Initialized window");

    constexpr size_t TotalGameMemory = 4ll * 1024ll * 1024ll;
    auto gameMemory = reinterpret_cast<uint8_t*>(malloc(TotalGameMemory));
    memset(gameMemory, 0, TotalGameMemory);

    auto arena = Arena::Create("ARENA_Base", gameMemory, TotalGameMemory);

    appInstance = arena->Push<Application>();

    auto remainingSize = arena->GetCapacity() - arena->GetUsedSize() - 1ll;
    auto appArena = arena->PushArena("ARENA_App", remainingSize);

    appInstance->Initialize(appArena, &hwnd);

    LOG("Initialized Application");

    LOG("Starting main loop");
    bool running = true;
    std::chrono::high_resolution_clock::time_point lastFrameTime(std::chrono::high_resolution_clock::now());

    while (running)
    {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto deltaTime = std::chrono::duration<float, std::milli>(now - lastFrameTime).count();
        lastFrameTime = now;

        MSG msg = {};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        appInstance->Update(deltaTime);
        char buffer[1024];
        for (size_t i = 0; i < Arena::sNumArenas; i++)
        {
            Arena* a = Arena::sArenas[i];
            auto bufferEnd = std::format_to(buffer, "{}: {}kb/{}kb", a->mName, a->GetUsedSize() / 1024ll, a->GetCapacity() / 1024ll);
            appInstance->AddDebugText(std::string_view(buffer, bufferEnd), 10, 100 + static_cast<int32_t>(i) * 16);
        }

        appInstance->Render();
    }

    DestroyWindow(hwnd);
}