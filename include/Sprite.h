#pragma once

#include <Common.h>
#include <Renderer.h>

#include <span>

class Sprite
{
public:
    Sprite() = default;
    ~Sprite() = default;

    void Initialize(float width, float height, std::span<TextureRef> frames, float frameTime = 0.0f);
    void AddTexture(TextureRef texture);

    void RenderFrame(Renderer* renderer, float x, float y, size_t frame, bool flipX = false, bool flipY = false);

    float GetWidth() const { return mWidth; }
    float GetHeight() const { return mHeight; }
    float GetFrameTime() const { return mFrameTime; }
    size_t GetFrameCount() const { return mFrames.size(); }

private:
    float mWidth;
    float mHeight;
    float mFrameTime;
    std::span<TextureRef> mFrames;
};
