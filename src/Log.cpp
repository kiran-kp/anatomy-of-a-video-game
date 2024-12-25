#include <Log.h>
#include <Util.h>

#include <cstdarg>

class LogMessage
{
public:
    LogMessage();
    void Set(std::string_view file, int line, uint64_t timestamp, uint64_t threadId, std::string_view format, va_list args);

    std::string mFile;
    int mLine;
    uint64_t mTimestamp;
    uint64_t mThreadId;
    std::string mMessage;
    std::atomic<LogMessage*> mNext;
    std::atomic<bool> mFree;
};

thread_local std::array<LogMessage, 512> Logger::mMessagePool;
thread_local size_t Logger::mMessagePoolIndex = 0;

LogMessage::LogMessage()
{
    mNext.store(nullptr);
    mFree.store(true);
}

void LogMessage::Set(std::string_view file, int line, uint64_t timestamp, uint64_t threadId, std::string_view format, va_list args)
{
    mFile = file;
    mLine = line;
    mTimestamp = timestamp;
    mThreadId = threadId;
    // Temporarily just format the string here. I eventually want to save the args as is and format offline.
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format.data(), args);
    mMessage = buffer;
}

Logger& Logger::Get()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
    auto dummy = new LogMessage();
    dummy->Set("<placeholder>", 0, 0, 0, "<placeholder>", nullptr);
    dummy->mNext.store(nullptr);
    mHead.store(dummy);
    mTail = dummy;

    auto err = fopen_s(&mFile, "log.txt", "w");
}

void Logger::Log(std::string_view file, int line, uint64_t timestamp, uint64_t threadId, std::string_view format, ...)
{
    LogMessage* message = nullptr;
    while (true)
    {
        message = &mMessagePool[mMessagePoolIndex];
        mMessagePoolIndex = (mMessagePoolIndex + 1) % mMessagePool.size();
        bool t = true; 
        if (message->mFree.compare_exchange_strong(t, false))
        {
            break;
        }
    }

    va_list args;
    va_start(args, format);
    message->Set(file, line, timestamp, threadId, format, args);
    va_end(args);

    // Enqueue message
    auto prev = mHead.exchange(message);
    prev->mNext.store(message);
}

void Logger::ProcessQueue()
{
    while (true)
    {
        auto tail = mTail;
        auto next = tail->mNext.load();
        if (next != nullptr)
        {
            tail->mFree.store(true);
            mTail = next;

            char buffer[4096];
            snprintf(buffer, sizeof(buffer), "%s(%d) ts=%lld %s\n", next->mFile.c_str(), next->mLine, next->mTimestamp, next->mMessage.c_str());

            printf(buffer);
            fprintf(mFile, buffer);
        }
        else
        {
            break;
        }
    }

    fflush(mFile);
}
