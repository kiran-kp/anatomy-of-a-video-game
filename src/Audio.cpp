#include <Audio.h>
#include <Util.h>

#include <xaudio2.h>

struct Sound;

class VoiceCallback : public IXAudio2VoiceCallback
{
public:
    Sound* mSnd = nullptr;

    VoiceCallback() = default;
    ~VoiceCallback() = default;

    //Called when the voice has just finished playing a contiguous audio stream.
    void OnStreamEnd() {}

    //Unused methods are stubs
    void OnVoiceProcessingPassEnd() noexcept {}
    void OnVoiceProcessingPassStart(UINT32 SamplesRequired) noexcept {}
    void OnBufferEnd(void* pBufferContext) noexcept ;
    void OnBufferStart(void* pBufferContext) noexcept {}
    void OnLoopEnd(void* pBufferContext) noexcept {}
    void OnVoiceError(void* pBufferContext, HRESULT Error) noexcept {}
};

struct Sound
{
    WAVEFORMATEXTENSIBLE wfx;
    XAUDIO2_BUFFER buffer;
    IXAudio2SourceVoice* sourceVoice;
    VoiceCallback callback;
    bool playing;
};

struct Impl
{
    IXAudio2* xaudio2;
    IXAudio2MasteringVoice* masteringVoice;
};

void VoiceCallback::OnBufferEnd(void* pBufferContext) noexcept
{
    Sound* snd = reinterpret_cast<Sound*>(pBufferContext);
    snd->sourceVoice->Stop();
    snd->playing = false;
}

void Audio::Initialize(Arena* arena)
{
    mArena = arena;
    auto impl = mArena->Push<Impl>();
    ensure(SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
    ensure(SUCCEEDED(::XAudio2Create(&impl->xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)));
    ensure(SUCCEEDED(impl->xaudio2->CreateMasteringVoice(&impl->masteringVoice)));

    mImpl = reinterpret_cast<uintptr_t>(impl);
}

#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'

HRESULT FindChunk(HANDLE hFile, DWORD fourcc, DWORD& dwChunkSize, DWORD& dwChunkDataPosition)
{
    HRESULT hr = S_OK;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, 0, NULL, FILE_BEGIN))
        return HRESULT_FROM_WIN32(GetLastError());

    DWORD dwChunkType;
    DWORD dwChunkDataSize;
    DWORD dwRIFFDataSize = 0;
    DWORD dwFileType;
    DWORD bytesRead = 0;
    DWORD dwOffset = 0;

    while (hr == S_OK)
    {
        DWORD dwRead;
        if (0 == ReadFile(hFile, &dwChunkType, sizeof(DWORD), &dwRead, NULL))
            hr = HRESULT_FROM_WIN32(GetLastError());

        if (0 == ReadFile(hFile, &dwChunkDataSize, sizeof(DWORD), &dwRead, NULL))
            hr = HRESULT_FROM_WIN32(GetLastError());

        switch (dwChunkType)
        {
        case fourccRIFF:
            dwRIFFDataSize = dwChunkDataSize;
            dwChunkDataSize = 4;
            if (0 == ReadFile(hFile, &dwFileType, sizeof(DWORD), &dwRead, NULL))
                hr = HRESULT_FROM_WIN32(GetLastError());
            break;

        default:
            if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, dwChunkDataSize, NULL, FILE_CURRENT))
                return HRESULT_FROM_WIN32(GetLastError());
        }

        dwOffset += sizeof(DWORD) * 2;

        if (dwChunkType == fourcc)
        {
            dwChunkSize = dwChunkDataSize;
            dwChunkDataPosition = dwOffset;
            return S_OK;
        }

        dwOffset += dwChunkDataSize;

        if (bytesRead >= dwRIFFDataSize) return S_FALSE;

    }

    return S_OK;
}

HRESULT ReadChunkData(HANDLE hFile, void* buffer, DWORD buffersize, DWORD bufferoffset)
{
    HRESULT hr = S_OK;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, bufferoffset, NULL, FILE_BEGIN))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    DWORD dwRead;
    if (0 == ReadFile(hFile, buffer, buffersize, &dwRead, NULL))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    return hr;
}

Audio::Ref Audio::LoadSound(std::string_view path)
{
    Impl* impl = reinterpret_cast<Impl*>(mImpl);
    auto snd = mArena->Push<Sound>();
    memset(snd, 0, sizeof(snd));
    new (snd) Sound();

    wchar_t wpath[1024];
    swprintf(wpath, 1024, L"%.*hs", static_cast<int>(path.size()), path.data());

    // Open the file
    HANDLE hFile = CreateFile(wpath,
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);

    ensure(INVALID_HANDLE_VALUE != hFile);
    ensure(INVALID_SET_FILE_POINTER != SetFilePointer(hFile, 0, NULL, FILE_BEGIN));

    DWORD dwChunkSize;
    DWORD dwChunkPosition;
    //check the file type, should be fourccWAVE or 'XWMA'
    FindChunk(hFile, fourccRIFF, dwChunkSize, dwChunkPosition);
    DWORD filetype;
    ReadChunkData(hFile, &filetype, sizeof(DWORD), dwChunkPosition);
    ensure(filetype == fourccWAVE);

    FindChunk(hFile, fourccFMT, dwChunkSize, dwChunkPosition);
    ReadChunkData(hFile, &snd->wfx , dwChunkSize, dwChunkPosition);

    //fill out the audio data buffer with the contents of the fourccDATA chunk
    FindChunk(hFile, fourccDATA, dwChunkSize, dwChunkPosition);
    std::span<BYTE> dataBuffer = mArena->PushArray<BYTE>(dwChunkSize);
    ReadChunkData(hFile, dataBuffer.data(), dwChunkSize, dwChunkPosition);

    snd->buffer.AudioBytes = static_cast<uint32_t>(dataBuffer.size());
    snd->buffer.pAudioData = dataBuffer.data();
    snd->buffer.Flags = XAUDIO2_END_OF_STREAM;
    snd->buffer.pContext = snd;

    new (&snd->callback) VoiceCallback();
    snd->callback.mSnd = snd;
    ensure(SUCCEEDED(impl->xaudio2->CreateSourceVoice(&snd->sourceVoice, (WAVEFORMATEX*)&snd->wfx, 0, 2.0f, &snd->callback)));

    return { reinterpret_cast<uintptr_t>(snd) };
}

void Audio::Play(Audio::Ref sound)
{
    Sound* snd = reinterpret_cast<Sound*>(sound.ptr);
    if (!snd->playing)
    {
        snd->playing = true;
        ensure(SUCCEEDED(snd->sourceVoice->SubmitSourceBuffer(&snd->buffer)));
        snd->sourceVoice->Start(0);
    }
}
