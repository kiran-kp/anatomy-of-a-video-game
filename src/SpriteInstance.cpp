#include <SpriteInstance.h>

void SpriteInstance::Initialize(const Sprite* sprite)
{
    mSprite = sprite;
    mFrame = 0;
    mTime = mSprite->GetFrameTime();
}

void SpriteInstance::Update(float deltaTime)
{
    if (mSprite->GetFrameTime() > 0.0f)
    {
        mTime -= deltaTime;

        if (mTime <= 0.0f)
        {
            mTime = mSprite->GetFrameTime();
            mFrame = (mFrame + 1) % mSprite->GetTextureCount();
        }
    }
}

void SpriteInstance::Render(Renderer& renderer)
{
    mSprite->RenderFrame(renderer, mX, mY, mFrame, mFlipX, mFlipY);
}
