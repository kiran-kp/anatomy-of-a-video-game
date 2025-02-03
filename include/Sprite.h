#include <Renderer.h>

#include <vector>

class Sprite
{
public:
    Sprite() = default;
    ~Sprite() = default;

    void Initialize(float width, float height, float frameTime = 0.0f);
    void AddTexture(TextureRef texture);
    void Render(Renderer& renderer, float deltaTime, float x, float y);

private:
    float mWidth;
    float mHeight;
    size_t mFrame;
    float mFrameTime;
    float mTime;
    std::vector<TextureRef> mTextures;
};