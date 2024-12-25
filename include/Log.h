#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#define LOG(format, ...) Logger::Get().Log(__FILE__, __LINE__, std::chrono::high_resolution_clock::now().time_since_epoch().count(), 0, format, ##__VA_ARGS__)
#define LOGGER_FLUSH() Logger::Get().ProcessQueue()

class LogMessage;

// Logger is a singleton that writes to a file and a stdout.
// Goals:
//    - Thread safe and lock free
//    - Does not allocate memory
//    - Queue processing done on a separate thread
//    - Format strings are processesed offline to improve runtime performance
class Logger
{
public:
    static Logger& Get();

    void Log(std::string_view file, int line, uint64_t timestamp, uint64_t threadId, std::string_view format, ...);
    void ProcessQueue();

private:
    Logger();
    FILE* mFile;
    thread_local static std::array<LogMessage, 512> mMessagePool;
    thread_local static size_t mMessagePoolIndex;
    std::atomic<LogMessage*> mHead;
    LogMessage* mTail;
};
