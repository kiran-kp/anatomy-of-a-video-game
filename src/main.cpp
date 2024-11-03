#include <windows.h>

#include <assert.h>

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            // Handle key down events
            return 0;
        case WM_KEYUP:
            // Handle key up events
            return 0;
        case WM_PAINT:
            // This will eventually be replaced with our game rendering code
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hWnd, &ps);
            return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[]  = L"BirdGame";

    // Initialize and register the window class
    WNDCLASS wc = { };

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create the window using the registered window class
    HWND hWnd = CreateWindowEx(0,
                               CLASS_NAME,
                               L"Bird Game",
                               WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               288, 512,                          // This width and height matches the resolution of our background image
                               NULL,
                               NULL,
                               hInstance,
                               NULL);

    assert (hWnd != NULL);

    ShowWindow(hWnd, nCmdShow);

    // Main message loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return static_cast<char>(msg.wParam);
}