#pragma once

#include <windows.h>

class Window
{
public:
    Window();
    ~Window();

    void Initialize(const wchar_t *title, int windowWidth, int windowHeight, HINSTANCE hInstance, int nCmdShow);
    void Shutdown();

    bool ProcessMessages();

    HWND GetHandle() const { return mHwnd; }

private:
    HWND mHwnd;
};