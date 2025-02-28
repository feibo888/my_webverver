//
// Created by fb on 2/23/25.
//

#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0)
{
}

size_t Buffer::WriteableBytes() const
{
    return buffer_.size() - writePos_;
}

size_t Buffer::ReadableBytes() const
{
    return writePos_ - readPos_;
}

size_t Buffer::PrependableBytes() const
{
    return readPos_;
}

const char* Buffer::Peek() const
{
    return BeginPtr_() + readPos_;
}

void Buffer::EnsureWriteable(size_t len)
{
    if (WriteableBytes() < len)
    {
        MakeSpace_(len);
    }
    assert(WriteableBytes() >= len);
}

void Buffer::HasWritten(size_t len)
{
    writePos_ += len;
}

void Buffer::Retrieve(size_t len)
{
    assert(len <= ReadableBytes());
    readPos_ += len;
}

void Buffer::RetrieveUntil(const char* end)
{
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

void Buffer::RetrieveAll()
{
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr()
{
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char* Buffer::BeginWriteConst() const
{
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite()
{
    return BeginPtr_() + writePos_;
}

void Buffer::Append(const std::string& str)
{
    Append(str.data(), str.length());
}

void Buffer::Append(const char* str, size_t len)
{
    assert(str);
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const void* data, size_t len)
{
}

void Buffer::Append(const Buffer& buff)
{
}

ssize_t Buffer::ReadFd(int fd, int* saveErrno)
{
    //将readv的数据保存到buffer中
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WriteableBytes();
    //分散读，保证数据全部读完
    //缓冲区是按数组顺序处理的，也就是说，只有在iov[0]被填满之后，才会去填充iov[1]
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = writable;

    const ssize_t len = readv(fd, iov, 2);
    if (len < 0)
    {
        *saveErrno = errno;
    }
    else if (static_cast<size_t>(len) <= writable)  //读取的长度没有超过buffer的容量就直接更新writePos_
    {
        writePos_ += len;
    }
    else
    {
        //读取的长度超过了buffer的容量，有两种解决办法：
        //1、如果len - writeable < 已经读过的位置，则可以将len - writeable这段数据拷到开头
        //2、否则可以resize buffer的大小
        writePos_ = buffer_.size();
        Append(buff, len - writable);
    }
    return len;

}

ssize_t Buffer::WriteFd(int fd, int* saveErrno)
{
    //将buffer中的数据发送出去
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if (len < 0)
    {
        *saveErrno = errno;
        return len;
    }
    readPos_ += len;
    return len;
}

char* Buffer::BeginPtr_()
{
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const
{
    return &*buffer_.begin();
}

void Buffer::MakeSpace_(size_t len)
{
    if (WriteableBytes() + PrependableBytes() < len)
    {
        buffer_.resize(writePos_ + len + 1);
    }
    else
    {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}
