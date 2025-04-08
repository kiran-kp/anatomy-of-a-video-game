#include <Common.h>
#include <Application.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    constexpr size_t TotalGameMemory = 128ll * 1024ll * 1024ll;
    auto gameMemory = reinterpret_cast<uint8_t*>(malloc(TotalGameMemory));
    memset(gameMemory, 0, TotalGameMemory);

    auto arena = Arena::Create("ARENA_Base", gameMemory, TotalGameMemory);

    auto app = reinterpret_cast<Application*>(arena->Push(sizeof(Application)));

    auto remainingSize = arena->GetCapacity() - arena->GetUsedSize() - 1ll;
    auto appArena = arena->PushArena("ARENA_App", remainingSize);

    new (app) Application(appArena, hInstance, nCmdShow);

    app->Run();
}