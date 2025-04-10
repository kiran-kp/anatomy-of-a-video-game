#include <Sprite.h>

void Sprite::Initialize(float width, float height, std::span<TextureRef> frames, float frameTime)
{
    mWidth = width;
    mHeight = height;
    mFrameTime = frameTime;
    mFrames = frames;
}

void Sprite::RenderFrame(Renderer* renderer, float x, float y, size_t frame, bool flipX, bool flipY)
{
    ensure(!mFrames.empty());
    ensure(frame < mFrames.size());

    renderer->AddQuad(x, y, mWidth, mHeight, flipX, flipY, mFrames[frame]);
}
