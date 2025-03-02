#pragma once

#include <Renderer.h>
#include <vector>

class Sprite
{
public:
    Sprite() = default;
    ~Sprite() = default;

    void Initialize(float width, float height, float frameTime = 0.0f);
    void AddTexture(TextureRef texture);

    void RenderFrame(Renderer& renderer, float x, float y, size_t frame, bool flipX = false, bool flipY = false) const;

    float GetWidth() const { return mWidth; }
    float GetHeight() const { return mHeight; }
    float GetFrameTime() const { return mFrameTime; }
    size_t GetTextureCount() const { return mTextures.size(); }

private:
    float mWidth;
    float mHeight;
    float mFrameTime;
    std::vector<TextureRef> mTextures;
};
