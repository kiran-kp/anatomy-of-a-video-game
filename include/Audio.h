#pragma once

#include <memory>
#include <string_view>

class Audio
{
public:
    struct Ref
    {
        size_t id;
    };

    Audio();
    ~Audio();

    void Initialize();

    Ref LoadSound(std::string_view path);
    void Play(Ref sound);

private:
    struct alignas(8) implT
    {
        char data[624];
    };

    implT mImpl;
};