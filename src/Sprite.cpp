#include <Sprite.h>

#include <cassert>

void Sprite::Initialize(float width, float height, float frameTime)
{
    mWidth = width;
    mHeight = height;
    mFrame = 0;
    mFrameTime = frameTime;
}

void Sprite::AddTexture(TextureRef texture)
{
    mTextures.push_back(texture);
}

void Sprite::Update(float deltaTime)
{
    if (mFrameTime > 0.0f)
    {
        mTime -= deltaTime;

        if (mTime <= 0.0f)
        {
            mTime = mFrameTime;
            mFrame = (mFrame + 1) % mTextures.size();
        }
    }
}

void Sprite::Render(Renderer& renderer, float x, float y)
{
    assert(!mTextures.empty());

    renderer.AddQuad(x, y, mWidth, mHeight, mTextures[mFrame]);
}
