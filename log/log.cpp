//
// Created by fb on 2/26/25.
//
#include "log.h"

#include <stdarg.h>
#include <sys/stat.h>

using namespace std;


void Log::init(int level = 1, const char* path, const char* suffix, int maxQueueCapacity)
{
    isOpen_ = true;
    level_ = level;
    //isAsync_设置为 true，表示日志将通过队列异步写入。
    if (maxQueueCapacity > 0)
    {
        isAsync_ = true;
        if (!deque_)
        {
            //存放数据
            std::unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = std::move(newDeque);

            //存放写线程
            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_ = std::move(NewThread);
        }
    }
    else
    {
        //日志将直接写入文件，不使用队列
        isAsync_ = false;
    }

    lineCount_ = 0;

    time_t timer = time(nullptr);   //获取当前时间戳
    struct tm* sysTime = localtime(&timer);     //转换为当前时间
    struct tm t = *sysTime;         //保存时间副本
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        //关闭旧文件
        //如果 fp_ 已打开，先调用 flush() 将缓冲数据写入文件，再用 fclose(fp_) 关闭。
        // if (fp_)
        // {
        //     flush();
        //     fclose(fp_);
        // }

        if (file_.is_open())  // 如果文件已打开，先关闭
        {
            file_.flush();
            file_.close();
        }
        

        //追加模式打开
        // fp_ = fopen(fileName, "a");
        // if (fp_ == nullptr) //打开失败则创建目录，赋予权限777，以便能在path下创建文件
        // {
        //     mkdir(path_, 0777);
        //     fp_ = fopen(fileName, "a");
        // }
        // assert(fp_ != nullptr);

        // 以追加模式打开文件
        file_.open(fileName, std::ios::app);
        if (!file_.is_open())  // 打开失败则创建目录并重试
        {
            mkdir(path_, 0777);
            file_.open(fileName, std::ios::app);
        }
        assert(file_.is_open());
    }

}

Log* Log::Instance()
{
    static Log inst;
    return &inst;
}

void Log::FlushLogThread()
{
    Log::Instance()->AsyncWrite_();
}

void Log::write(int level, const char* format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);       //获取当前时间，now.tv_sec 是秒数，now.tv_usec 是微秒数。
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;         //将 sysTime 的内容复制到 t 中，避免直接修改 sysTime。
    va_list vaList;         //定义可变参数列表变量，用于后续处理 format 中的参数。

    //日志日期，日志行数
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        // 生成新文件名，例如 /path/2023_10_16.log，其中 tail 是日期部分（如 2023_10_16）。
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday)
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else
        {
            // 生成带编号的文件名，例如 /path/2023_10_16-1.log，编号为 lineCount_ / MAX_LINES。
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_ / MAX_LINES), suffix_);
        }

        locker.lock();
        // flush();
        // fclose(fp_);
        // fp_ = fopen(newFile, "a");
        // assert(fp_ != nullptr);

        file_.flush();
        file_.close();
        file_.open(newFile, std::ios::app);
        assert(file_.is_open());
    }

    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        buff_.HasWritten(n);

        //添加 [INFO] 等前缀
        AppendLogLevelTitle(level);

        // 处理可变参数
        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WriteableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        //buff_.Append("\n\0", 2);
        buff_.Append("\n", 1);

        if (isAsync_ && deque_ && !deque_->full())
        {
            // 如果启用且队列未满，推入队列 deque_。
            //将buff中还没有读取的数据拿出来，然后清空buff
            //cout << "Pushing LogID: " << lineCount_ << " to queue" << endl; // 调试输出
            deque_->push_back(buff_.RetrieveAllToStr());
        }
        else
        {
            // 直接用 fputs 写入文件
            //fputs(buff_.Peek(), fp_);
        
            file_ << buff_.Peek();  // 直接写入文件流

            //fputs(buff_.Peek(), fp_);
        }
        // 重置缓冲区
        buff_.RetrieveAll();
    }
}

void Log::flush()
{
    if (isAsync_)
    {
        deque_->flush();
    }
    //fflush(fp_);
    file_.flush();  // 刷新文件流
}

int Log::GetLevel()
{
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level)
{
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

Log::Log() : lineCount_(0), isAsync_(false), writeThread_(nullptr), deque_(nullptr), toDay_(0)
{
}

void Log::AppendLogLevelTitle(int level)
{
    //string str;
    switch (level)
    {
        case 0: buff_.Append("[DEBUG]: ", 9); break;
        case 1: buff_.Append("[INFO] : ", 9); break;
        case 2: buff_.Append("[WARN] : ", 9); break;
        case 3: buff_.Append("[ERROR]: ", 9); break;
        default: buff_.Append("[INFO] : ", 9); break;
    }
    //buff_.Append(str.c_str(), str.length());
}

Log::~Log()
{
    if (writeThread_ && writeThread_->joinable())
    {
        while (!deque_->empty())
        {
            deque_->flush();
        }
        deque_->Close();
        writeThread_->join();
    }
    // if (fp_)
    // {
    //     lock_guard<mutex> locker(mtx_);
    //     flush();
    //     fclose(fp_);
    // }

    if (file_.is_open())
    {
        lock_guard<mutex> locker(mtx_);
        file_.flush();
        file_.close();
    }
}

void Log::AsyncWrite_()
{
    string str = "";
    while (deque_->pop(str))
    {
        lock_guard<mutex> locker(mtx_);     //虽然写线程只有一个，但是当队列满的时候主线程会直接写文件，所以需要上锁
        //fputs(str.c_str(), fp_);
        file_ << str;  // 异步写入文件流
        //file_.flush();
    }
}
