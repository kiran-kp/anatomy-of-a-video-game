#pragma once

#include <bit>
#include <cstdint>
#include <span>

#define ensure(x) if (!(x)) { int *y = 0; *y = 42; }

constexpr uint32_t WindowWidth = 288 * 2;
constexpr uint32_t WindowHeight = 512 * 2;

struct Vec2
{
    float x;
    float y;
};

struct Rect
{
    union
    {
        struct
        {
            Vec2 pos;
            Vec2 size;
        };
        
        struct
        {
            float x;
            float y;
            float w;
            float h;
        };
    };
};

class Arena
{
public:
    static Arena* Create(const char* name, uint8_t* backingMemory, size_t capacity);

    uint8_t* Push(size_t size, size_t alignment = 8);

    template <typename T> std::span<T> PushArray(size_t size)
    {
        uint8_t* memory = Push(sizeof(T) * size);
        return std::span(reinterpret_cast<T*>(memory), size);
    }

    Arena* PushArena(const char* name, size_t capacity);
    void Clear();

    size_t GetUsedSize() const;
    size_t GetCapacity() const;

private:
    Arena()  = default;
    ~Arena() = default;

    char mName[16];
    uint8_t* mBase = nullptr;
    size_t mOffset = 0;
    size_t mCapacity = 0;
};

constexpr size_t AlignPow2(size_t value, size_t alignment)
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

constexpr size_t NextPow2(size_t value)
{
    return std::bit_ceil(value);
}

constexpr size_t ArenaHeaderSize = NextPow2(sizeof(Arena));
