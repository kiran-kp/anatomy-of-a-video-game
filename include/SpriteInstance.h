#pragma once

#include <Sprite.h>

class SpriteInstance
{
public:
    SpriteInstance() = default;
    ~SpriteInstance() = default;

    void Initialize(const Sprite* sprite);
    void Update(float deltaTime);
    void Render(Renderer& renderer);

    void SetPosition(float x, float y) { mX = x; mY = y; }
    void SetFlip(bool flipX, bool flipY) { mFlipX = flipX; mFlipY = flipY; }

private:
    const Sprite* mSprite;
    float mX;
    float mY;
    bool mFlipX;
    bool mFlipY;
    size_t mFrame;
    float mTime;
};
