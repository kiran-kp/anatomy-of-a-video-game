#include <Log.h>
#include <Util.h>

#include <cstdarg>

class LogMessage
{
public:
    LogMessage(std::string_view file, int line, uint64_t timestamp, uint64_t threadId, std::string_view format, va_list args);

    std::string mFile;
    int mLine;
    uint64_t mTimestamp;
    uint64_t mThreadId;
    std::string mMessage;
    std::atomic<LogMessage*> mNext;
};

LogMessage::LogMessage(std::string_view file, int line, uint64_t timestamp, uint64_t threadId, std::string_view format, va_list args)
    : mFile(file), mLine(line), mTimestamp(timestamp), mThreadId(threadId)
{
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
    auto dummy = new LogMessage("", 0, 0, 0, "", nullptr);
    dummy->mNext.store(nullptr);
    mHead.store(dummy);
    mTail = dummy;

    auto err = fopen_s(&mFile, "log.txt", "w");
}

void Logger::Log(std::string_view file, int line, uint64_t timestamp, uint64_t threadId, std::string_view format, ...)
{
    va_list args;
    va_start(args, format);
    LogMessage* message = new LogMessage(file, line, timestamp, threadId, format, args);
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
            delete tail;
            mTail = next;

            char buffer[4096];
            snprintf(buffer, sizeof(buffer), "[%lld] %s:%d - %s\n", next->mTimestamp, next->mFile.c_str(), next->mLine, next->mMessage.c_str());

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
