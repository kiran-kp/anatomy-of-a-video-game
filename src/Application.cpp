#include <Application.h>
#include <Log.h>
#include <Util.h>

#include <png.h>

#include <cassert>
#include <format>
#include <string_view>
#include <thread>
#include <vector>
#include <unordered_map>

std::unique_ptr<Application> Application::mInstance;

struct Image
{
    std::string path;
    float width;
    float height;
};

Application::Application()
    : mWindow()
    , mRenderer()
    , mBackground()
{
}

Application::~Application()
{
}

// Useful debugging tool: https://www.nayuki.io/page/png-file-chunk-inspector
std::vector<uint8_t> ReadPNG(std::string_view path)
{
    FILE* file = nullptr;
    if ((fopen_s(&file, path.data(), "rb") == 0) && !file)
    {
        return {};
    }

    char header[8];
    fread(header, 1, 8, file);

    fseek(file, 0, SEEK_SET);

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png)
    {
        fclose(file);
        return {};
    }

    png_infop info = png_create_info_struct(png);
    if (!info)
    {
        png_destroy_read_struct(&png, nullptr, nullptr);
        fclose(file);
        return {};
    }

    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_read_struct(&png, &info, nullptr);
        fclose(file);
        return {};
    }

    png_init_io(png, file);

    png_read_png(png, info, PNG_TRANSFORM_IDENTITY, nullptr);

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, compression_type, filter_method;
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_method);

    assert (color_type == PNG_COLOR_TYPE_PALETTE);

    png_colorp palette;
    int numPalette;
    png_get_PLTE(png, info, &palette, &numPalette);
    png_bytep transAlpha = nullptr;
    int32_t numTransAlpha = 0;
    png_color_16p transColors = nullptr;
    png_get_tRNS(png, info, &transAlpha, &numTransAlpha, &transColors);

    std::vector<uint8_t> textureData(width * height * 4);
    png_bytepp pngData = png_get_rows(png, info);
    const int pixelsPerByte = 8 / bit_depth;
    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            png_bytep row = pngData[y];
            png_byte pixelGroup = row[x / pixelsPerByte];
            png_byte pixel = 0;
            if (bit_depth == 4)
            {
                pixel = (pixelGroup >> (4 - (x % 2) * 4)) & 0xF;
            }
            else if (bit_depth == 8)
            {
                pixel = row[x];
            }
            else
            {
                ensure(false);
            }


            png_color color = palette[pixel];
            size_t base = (y * width + x) * 4;
            textureData[base + 0] = color.red;
            textureData[base + 1] = color.green;
            textureData[base + 2] = color.blue;
            textureData[base + 3] = transAlpha ? transAlpha[pixel] : 0xFF;
        }
    }

    png_destroy_read_struct(&png, &info, nullptr);
    fclose(file);

    return textureData;
}

void Application::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    auto log_thread = std::thread([]() {
        using namespace std::chrono_literals;
        while (true)
        {
            LOGGER_FLUSH();
            std::this_thread::sleep_for(10ms);
        }
    });

    log_thread.detach();

    mInstance.reset(new Application());
    mInstance->mWindow.Initialize(L"Bird Game", 288, 512, hInstance, nCmdShow);
    LOG("Initialized Window");
    mInstance->mRenderer.Initialize(mInstance->mWindow);
    LOG("Initialized Renderer");

    std::vector<std::pair<Sprite*, Image>> images = {
        { &mInstance->mBird, { "assets/sprites/bluebird-downflap.png", 34.0f, 24.0f } },
        { &mInstance->mBird, { "assets/sprites/bluebird-midflap.png", 34.0f, 24.0f } },
        { &mInstance->mBird, { "assets/sprites/bluebird-upflap.png", 34.0f, 24.0f } },
        { &mInstance->mBackground, { "assets/sprites/background-night.png", 288.0f, 512.0f } },
        { &mInstance->mPipe, { "assets/sprites/pipe-green.png", 52.0f, 320.0f } },
        { &mInstance->mBase, { "assets/sprites/base.png", 336.0f, 112.0f } },
        { &mInstance->mGameOver, { "assets/sprites/gameover.png", 192.0f, 42.0f } }
    };

    for (auto& [sprite, image] : images)
    {
        auto data = ReadPNG(image.path);
        sprite->Initialize(image.width, image.height, 160.0f);
        sprite->AddTexture(mInstance->mRenderer.CreateTexture(static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), 4, data.data()));
    }

    LOG("Initialized Textures");
    mInstance->mRenderer.FinishUploadingTextures();
    LOG("Uploaded textures to GPU");

    mInstance->mLastFrameTime = std::chrono::high_resolution_clock::now();
}

Application& Application::Instance()
{
    return *mInstance;
}

void Application::Run()
{
    while (mWindow.ProcessMessages())
    {
        Update();
        Render();
    }
}

void Application::Update()
{
    const auto now = std::chrono::high_resolution_clock::now();
    const auto deltaTime = std::chrono::duration<float, std::milli>(now - mLastFrameTime).count();
    mLastFrameTime = now;

    mBird.Update(deltaTime);

    mRenderer.AddDebugText(std::format("Frame time: {:.4}", deltaTime), 100, 100);
}

void Application::Render()
{
    static float x = 50.0f;
    static float y = 50.0f;

    static float xDir = 1.0f;
    static float yDir = 1.0f;
    
    mBackground.Render(mRenderer, 0.0f, 0.0f);
    mBird.Render(mRenderer, x, y);

    x += 1.0f * xDir;
    y += 1.0f * yDir;

    if ((x + mBird.GetWidth()) > 288.0f || x < 0.0f)
    {
        xDir *= -1.0f;
    }

    if ((y + mBird.GetHeight()) > 512.0f || y < 0.0f)
    {
        yDir *= -1.0f;
    }

    mRenderer.Render();
}

void Application::KeyDown()
{
}

void Application::KeyUp()
{
}