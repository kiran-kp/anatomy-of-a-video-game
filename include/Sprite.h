#include <Renderer.h>

#include <vector>

class Sprite
{
public:
    Sprite() = default;
    ~Sprite() = default;

    void Initialize(float width, float height, float frameTime = 0.0f);
    void AddTexture(TextureRef texture);

    void Update(float deltaTime);
    void Render(Renderer& renderer, float x, float y);

    float GetWidth() const { return mWidth; }
    float GetHeight() const { return mHeight; }

private:
    float mWidth;
    float mHeight;
    size_t mFrame;
    float mFrameTime;
    float mTime;
    std::vector<TextureRef> mTextures;
};