#include <Common.h>
#include <Util.h>

std::span<Arena*> Arena::sArenas;
size_t Arena::sNumArenas = 0;

Arena* Arena::Create(const char* name, uint8_t* backingMemory, size_t capacity)
{
    auto arena = reinterpret_cast<Arena*>(backingMemory);
    strncpy_s(arena->mName, 16, name, strlen(name));
    arena->mBase = backingMemory;
    arena->mOffset = ArenaHeaderSize;
    arena->mCapacity = capacity;

    if (sNumArenas == 0)
    {
        sArenas = arena->PushArray<Arena*>(10);
    }

    sArenas[sNumArenas++] = arena;

    return arena;
}

uint8_t* Arena::Push(size_t size, size_t alignment/* = 8 */)
{
    size_t start = AlignPow2(mOffset, alignment);
    ensure((start + size) < mCapacity);
    mOffset = start + size;
    return mBase + start;
}

Arena* Arena::PushArena(const char* name, size_t capacity)
{
    uint8_t* memory = Push(capacity);
    auto newArena = Create(name, memory, capacity);
    return newArena;
}

void Arena::Clear()
{
    mOffset = ArenaHeaderSize;
}

size_t Arena::GetUsedSize() const
{
    return mOffset;
}

size_t Arena::GetCapacity() const
{
    return mCapacity;
}