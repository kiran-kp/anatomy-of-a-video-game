#include <Sprite.h>

#include <cassert>

void Sprite::Initialize(float width, float height, float frameTime)
{
    mWidth = width;
    mHeight = height;
    mFrameTime = frameTime;
}

void Sprite::AddTexture(TextureRef texture)
{
    mTextures.push_back(texture);
}

void Sprite::RenderFrame(Renderer& renderer, float x, float y, size_t frame, bool flipX, bool flipY) const
{
    assert(!mTextures.empty());
    assert(frame < mTextures.size());

    renderer.AddQuad(x, y, mWidth, mHeight, flipX, flipY, mTextures[frame]);
}
