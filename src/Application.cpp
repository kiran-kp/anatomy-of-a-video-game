#include <Application.h>
#include <Log.h>

#include <png.h>

#include <cassert>
#include <string_view>
#include <thread>
#include <vector>

std::unique_ptr<Application> Application::mInstance;

Application::Application()
    : mWindow()
    , mRenderer()
{
}

Application::~Application()
{
}

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
            assert(bit_depth == 4);
            png_byte pixel = (pixelGroup >> (4 - (x % 2) * 4)) & 0xF;

            png_color color = palette[pixel];
            size_t base = (y * width + x) * 4;
            textureData[base + 0] = color.red;
            textureData[base + 1] = color.green;
            textureData[base + 2] = color.blue;
            textureData[base + 3] = transAlpha[pixel];
        }
    }

    png_destroy_read_struct(&png, &info, nullptr);
    fclose(file);

    return textureData;
}

TextureRef bird1;
TextureRef bird2;
TextureRef bird3;

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

    {
        auto data = ReadPNG("assets/sprites/bluebird-downflap.png");
        bird1 = mInstance->mRenderer.CreateTexture(34, 24, 4, data.data());
    }
    {
        auto data = ReadPNG("assets/sprites/bluebird-midflap.png");
        bird2 = mInstance->mRenderer.CreateTexture(34, 24, 4, data.data());
    }
    {
        auto data = ReadPNG("assets/sprites/bluebird-upflap.png");
        bird3 = mInstance->mRenderer.CreateTexture(34, 24, 4, data.data());
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
	TextureRef bird[] = { bird1, bird2, bird3 };
    static size_t frame = 0;
	mRenderer.AddQuad(50.0f, 50.0f, 34.0f, 24.0f, bird[frame]);
	frame = (frame + 1) % 3;
    mRenderer.Render();
}

void Application::KeyDown()
{
}

void Application::KeyUp()
{
}