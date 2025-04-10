#include <SpriteInstance.h>

void SpriteInstance::Initialize(Sprite* sprite)
{
    mSprite = sprite;
    mFrame = 0;
    mPosition = { 0.0f, 0.0f };
    mFlipX = false;
    mFlipY = false;
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
            mFrame = (mFrame + 1) % mSprite->GetFrameCount();
        }
    }
}

void SpriteInstance::Render(Renderer* renderer)
{
    mSprite->RenderFrame(renderer, mPosition.x, mPosition.y, mFrame, mFlipX, mFlipY);
}
