//
// Created by fb on 12/27/24.
//


#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn()
{
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

HttpConn::~HttpConn()
{
    Close();
}

void HttpConn::init(int fd, const sockaddr_in& addr)
{
    assert(fd >= 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    writeBuf_.RetrieveAll();
    readBuf_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

ssize_t HttpConn::read(int* saveErrno)
{
    ssize_t len = -1;
    do
    {
        len = readBuf_.ReadFd(fd_, saveErrno);
        if (len <= 0)
        {
            break;
        }
    }while (isET);
    return len;
}

ssize_t HttpConn::write(int* saveErrno)
{
    ssize_t len = -1;
    do
    {
        //将iov的数据写到fd中
        len = writev(fd_, iov_, iovCnt_);
        if (len <= 0)
        {
            *saveErrno = errno;
            break;
        }
        //传输结束
        if (iov_[0].iov_len + iov_[1].iov_len == 0)
        {
            break;
        }
        else if (static_cast<size_t>(len) > iov_[0].iov_len)
        {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len)
            {
                writeBuf_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else
        {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuf_.Retrieve(len);
        }
    }while (isET || ToWriteBytes() > 10240);
    return len;
}

void HttpConn::Close()
{
    response_.UnmapFile();
    if (isClose_ == false)
    {
        isClose_ = true;
        userCount--;
        close(fd_);
    }
    LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

int HttpConn::GetFd() const
{
    return fd_;
}

int HttpConn::GetPort() const
{
    return addr_.sin_port;
}

const char* HttpConn::GetIP() const
{
    return inet_ntoa(addr_.sin_addr);   //将网络字节序地址转化为点分十进制表示形式
}

sockaddr_in HttpConn::GetAddr() const
{
    return addr_;
}

bool HttpConn::process()
{
    LOG_DEBUG("starting http conn process");
    request_.Init();
    if (readBuf_.ReadableBytes() <= 0)
    {
        return false;
    }
    else if (request_.parse(readBuf_))      //解析请求报文
    {
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    }
    else
    {
        response_.Init(srcDir, request_.path(), false, 400);
    }

    //生成响应报文
    response_.MakeResponse(writeBuf_);

    //响应头
    iov_[0].iov_base = const_cast<char*>(writeBuf_.Peek());
    iov_[0].iov_len = writeBuf_.ReadableBytes();
    iovCnt_ = 1;

    //文件
    if (response_.FileLen() > 0 && response_.File())
    {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }

    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
