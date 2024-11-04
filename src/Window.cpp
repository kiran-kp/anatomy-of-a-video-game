#include <Application.h>
#include <Window.h>

#include <assert.h>

Window::Window()
{
}

Window::~Window()
{
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    case WM_KEYDOWN:
    {
        Application::Instance().KeyDown(static_cast<uint8_t>(wParam));
        return 0;
    }
    case WM_KEYUP:
    {
        Application::Instance().KeyUp(static_cast<uint8_t>(wParam));
        return 0;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

void Window::Initialize(const wchar_t* title, int windowWidth, int windowHeight, HINSTANCE hInstance, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"BirdGame";

    // Initialize and register the window class
    WNDCLASS wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create the window using the registered window class
    mHwnd = CreateWindowEx(0,
                           CLASS_NAME,
                           title,
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           windowWidth, windowHeight, // This width and height matches the resolution of our background image
                           NULL,
                           NULL,
                           hInstance,
                           NULL);

    assert(mHwnd != NULL);

    ShowWindow(mHwnd, nCmdShow);
}

void Window::Shutdown()
{
    DestroyWindow(mHwnd);
}

bool Window::ProcessMessages()
{
    bool running = true;
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

    return running;
}