#pragma once

#include <Sprite.h>

struct Vec2
{
    float x;
    float y;
};


class SpriteInstance
{
public:
    SpriteInstance() = default;
    ~SpriteInstance() = default;

    void Initialize(const Sprite* sprite);
    void Update(float deltaTime);
    void Render(Renderer& renderer);

    Vec2 GetPosition() const { return mPosition; }
    void SetPosition(Vec2 pos) { mPosition = pos; }
    void SetFlip(bool flipX, bool flipY) { mFlipX = flipX; mFlipY = flipY; }
    bool GetYFlipped() const { return mFlipY; }

private:
    const Sprite* mSprite;
    Vec2 mPosition;
    bool mFlipX;
    bool mFlipY;
    size_t mFrame;
    float mTime;
};
