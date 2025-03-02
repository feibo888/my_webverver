//
// Created by fb on 12/25/24.
//

#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"
#include <fstream>

//log.h:

class Log
{
public:
    void init(int level, const char* path = "./log", const char* suffix = ".log", int maxQueueCapacity = 1024);

    static Log* Instance();
    static void FlushLogThread();

    void write(int level, const char* format, ...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    [[nodiscard]] bool isOpen() const { return isOpen_; }

private:
    Log();
    void AppendLogLevelTitle(int level);
    virtual ~Log();
    void AsyncWrite_();

private:
    static constexpr  int LOG_PATH_LEN = 256;
    static constexpr int LOG_NAME_LEN = 256;
    static constexpr int MAX_LINES = 50000;

    const char* path_;
    const char* suffix_;

    int MAX_LINES_;

    int lineCount_;
    bool isAsync_;

    std::unique_ptr<std::thread> writeThread_;
    std::unique_ptr<BlockDeque<std::string>> deque_;
    std::mutex mtx_;

    int toDay_;
    bool isOpen_;

    Buffer buff_;
    int level_;



    //FILE* fp_;
    std::ofstream file_;

};

#define LOG_BASE(level, format, ...) \
do{\
Log* log = Log::Instance();\
if(log->isOpen() && log->GetLevel() <= level){\
log->write(level, format, ##__VA_ARGS__);\
}\
}while(0);

#define LOG_DEBUG(format, ...) do{LOG_BASE(0, format, ##__VA_ARGS__)}while(0);
#define LOG_INFO(format, ...) do{LOG_BASE(1, format, ##__VA_ARGS__)}while(0);
#define LOG_WARN(format, ...) do{LOG_BASE(2, format, ##__VA_ARGS__)}while(0);
#define LOG_ERROR(format, ...) do{LOG_BASE(3, format, ##__VA_ARGS__)}while(0);

#endif //LOG_H
