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
    size_t numSounds;
    Sound sounds[5];
};

static_assert(sizeof(Impl) == sizeof(Audio::mImpl));

void VoiceCallback::OnBufferEnd(void* pBufferContext) noexcept
{
    Sound* snd = reinterpret_cast<Sound*>(pBufferContext);
    snd->sourceVoice->Stop();
    snd->playing = false;
}

Audio::Audio() = default;
Audio::~Audio() = default;

void Audio::Initialize()
{
    Impl* impl = reinterpret_cast<Impl*>(&mImpl);
    memset(impl, 0, sizeof(Impl));
    ensure(SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
    ensure(SUCCEEDED(::XAudio2Create(&impl->xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)));
    ensure(SUCCEEDED(impl->xaudio2->CreateMasteringVoice(&impl->masteringVoice)));
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
    Impl* impl = reinterpret_cast<Impl*>(&mImpl);
    size_t index = impl->numSounds;
    impl->numSounds += 1;
    Sound& snd = impl->sounds[index];
    memset(&snd, 0, sizeof(snd));

    std::wstring wpath(std::begin(path), std::end(path));
    // Open the file
    HANDLE hFile = CreateFile(wpath.c_str(),
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
    ReadChunkData(hFile, &snd.wfx , dwChunkSize, dwChunkPosition);

    //fill out the audio data buffer with the contents of the fourccDATA chunk
    FindChunk(hFile, fourccDATA, dwChunkSize, dwChunkPosition);
    BYTE* pDataBuffer = new BYTE[dwChunkSize];
    ReadChunkData(hFile, pDataBuffer, dwChunkSize, dwChunkPosition);

    snd.buffer.AudioBytes = dwChunkSize;
    snd.buffer.pAudioData = pDataBuffer;
    snd.buffer.Flags = XAUDIO2_END_OF_STREAM;
    snd.buffer.pContext = &snd;

    new (&snd.callback) VoiceCallback();
    snd.callback.mSnd = &snd;
    ensure(SUCCEEDED(impl->xaudio2->CreateSourceVoice(&snd.sourceVoice, (WAVEFORMATEX*)&snd.wfx, 0, 2.0f, &snd.callback)));

    return { index };
}

void Audio::Play(Audio::Ref sound)
{
    Impl* impl = reinterpret_cast<Impl*>(&mImpl);
    Sound& snd = impl->sounds[sound.id];
    if (!snd.playing)
    {
        snd.playing = true;
        ensure(SUCCEEDED(snd.sourceVoice->SubmitSourceBuffer(&snd.buffer)));
        snd.sourceVoice->Start(0);
    }
}
