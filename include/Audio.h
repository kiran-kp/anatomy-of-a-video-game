#pragma once

#include <Common.h>

#include <string_view>
#include <cstdint>

class Audio
{
public:
    struct Ref
    {
        uintptr_t ptr;
    };

    void Initialize(Arena* arena);

    Ref LoadSound(std::string_view path);
    void Play(Ref sound);

private:
    Audio() = delete;
    ~Audio() = delete;

    Arena* mArena;
    uintptr_t mImpl;
};