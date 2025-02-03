#include <Application.h>
#include <Log.h>
#include <Util.h>

#include <png.h>

#include <cassert>
#include <string_view>
#include <thread>
#include <vector>
#include <unordered_map>

std::unique_ptr<Application> Application::mInstance;

struct Image
{
    std::string path;
    uint32_t width;
    uint32_t height;
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

    std::unordered_map<std::string, Image> images = {
        { "bird-downflap", { "assets/sprites/bluebird-downflap.png", 34, 24 } },
        { "bird-midflap", { "assets/sprites/bluebird-midflap.png", 34, 24 } },
        { "bird-upflap", { "assets/sprites/bluebird-upflap.png", 34, 24 } },
        { "background", { "assets/sprites/background-night.png", 288, 512 } },
        { "pipe", { "assets/sprites/pipe-green.png", 52, 320 } },
        { "base", { "assets/sprites/base.png", 336, 112 } },
        { "game-over", { "assets/sprites/gameover.png", 192, 42 } }
    };



    mInstance->mBird.Initialize(34, 24, 0.1f);

    {
        auto data = ReadPNG("assets/sprites/bluebird-downflap.png");
        mInstance->mBird.AddTexture(mInstance->mRenderer.CreateTexture(34, 24, 4, data.data()));
    }
    
    {
        auto data = ReadPNG("assets/sprites/bluebird-midflap.png");
        mInstance->mBird.AddTexture(mInstance->mRenderer.CreateTexture(34, 24, 4, data.data()));
    }
    
    {
        auto data = ReadPNG("assets/sprites/bluebird-upflap.png");
        mInstance->mBird.AddTexture(mInstance->mRenderer.CreateTexture(34, 24, 4, data.data()));
    }
    
    mInstance->mBackground.Initialize(288, 512);
    {
        auto data = ReadPNG("assets/sprites/background-night.png");
        mInstance->mBackground.AddTexture(mInstance->mRenderer.CreateTexture(288, 512, 4, data.data()));
    }

    LOG("Initialized Textures");
    mInstance->mRenderer.FinishUploadingTextures();
    LOG("Uploaded textures to GPU");
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
    mRenderer.AddDebugText("Hello World!", 100, 100);
}

void Application::Render()
{
    mBackground.Render(mRenderer, 0.1f, 0.0f, 0.0f);
    mBird.Render(mRenderer, 0.1f, 50.0f, 50.0f);

    mRenderer.Render();
}

void Application::KeyDown()
{
}

void Application::KeyUp()
{
}