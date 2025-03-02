//
// Created by fb on 12/24/24.
//

#include "webserver.h"

#include <atomic>
using namespace std;

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger,
                     int sqlPort, const char* sqlUser, const char* sqlPwd,
                     const char* dbName, int connPoolNum, int threadNum,
                     bool openLog, int logLevel, int logQueSize):
                     port(port), openLinger(OptLinger), timeoutMS(timeoutMS), isClose(false),
                     timer(new HeapTimer()), threadpool(new ThreadPool(threadNum)), epoller(new Epoller())
{
    //资源路径（前端）
    srcDir = getcwd(nullptr, 256);
    assert(srcDir);
    strncat(srcDir, "/resources/", 16);

    //usercount原子操作，存储当前连接的用户数量
    //HttpConn::userCount.store(0);
    HttpConn::userCount = 0;
    HttpConn::srcDir = this->srcDir;

    //初始化连接池
    SqlConnPool::Instance()->init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    //设置触发模式
    InitEventMode_(trigMode);

    //初始化监听socket
    if (!InitSocket_())
    {
        isClose = true;
    }

    if(openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent & EPOLLET ? "ET": "LT"),
                            (connEvent & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }

}

WebServer::~WebServer()
{
    close(listenFd);
    isClose = true;
    free(srcDir);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::start()
{
    int timeMS = -1;        //=-1,无事件的话将阻塞
    if(!isClose) { LOG_INFO("========== Server start =========="); }
    while (!isClose)
    {
        if (timeoutMS > 0)      //timeoutMS表示每当有一个事件，必须要在timeoutMS内处理完
        {
            timeMS = timer->GetNextTick();  //获取下一个未超时事件与现在的时间之差，确保wait只等待这么多时间，不然来不及处理后面的任务
        }
        int eventCnt = epoller->Wait(timeMS);
        for (int i = 0; i < eventCnt; i++)
        {
            int fd = epoller->GetEventFd(i);
            uint32_t events = epoller->GetEvents(i);
            if (fd == listenFd)
            {
                DealListen_();
            }
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                assert(users.count(fd) > 0);
                CloseConn_(&users[fd]);
            }
            else if (events & EPOLLIN)
            {
                assert(users.count(fd) > 0);
                DealRead_(&users[fd]);
            }
            else if (events & EPOLLOUT)
            {
                assert(users.count(fd) > 0);
                DealWrite_(&users[fd]);
            }
            else
            {
                LOG_ERROR("Unexpected event");
            }
        }
    }

}

bool WebServer::InitSocket_()
{
    int ret;
    struct sockaddr_in addr;
    if (port > 65535 || port < 1024)
    {
        LOG_ERROR("Port:%d error!",  port);
        return false;
    }
    addr.sin_family = AF_INET;      //IPV4
    addr.sin_addr.s_addr = htonl(INADDR_ANY);       //本机的任何IP
    addr.sin_port = htons(port);        //host to net short 主机序转换为网络序，short类型

    //设置套接字在关闭时的处理方法
    struct linger optLinger = {0};
    if (openLinger)
    {
        //优雅关闭，直到所剩数据发送完毕或超时
        optLinger.l_onoff = 1;      //关闭套机字时阻塞，并尝试发送未发送完的数据
        optLinger.l_linger = 1;     //设置这个发送行为的超时时间为0s
    }

    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
    {
        LOG_ERROR("Create socket error!", port);
        return false;
    }

    // 设置延迟关闭
    ret = setsockopt(listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0)
    {
        close(listenFd);
        LOG_ERROR("Init linger error!", port);
        return false;
    }

    int optval = 1;
    //端口复用，只有最后一个套接字会正常接收数据
    //即使之前绑定到该端口的连接尚未完全关闭，服务器也可以重新绑定到同一端口。这对于在服务器重启时避免 "Address already in use" 错误非常有用。
    ret = setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1)
    {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd);
        return false;
    }

    ret = bind(listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0)
    {
        LOG_ERROR("Bind Port:%d error!", port);
        close(listenFd);
        return false;
    }

    // 监听socket，最大等待连接长度为6
    ret = listen(listenFd, 6);
    if (ret < 0)
    {
        LOG_ERROR("Listen port:%d error!", port);
        close(listenFd);
        return false;
    }

    // 把设置好的端口放入epoll中，并且只有读事件通知
    ret = epoller->AddFd(listenFd, listenEvent | EPOLLIN);  //EPOLLIN: 表示对应的文件描述符可以读
    if (ret == 0)
    {
        LOG_ERROR("Add listen error!");
        close(listenFd);
        return false;
    }

    SetFdNonblock(listenFd);    //将listenFd设置为非阻塞模式
    LOG_INFO("Server port:%d", port);
    return true;

}

void WebServer::InitEventMode_(int trigMode)
{
    listenEvent = EPOLLRDHUP;   //listen_event先赋值为关闭连接
    connEvent = EPOLLONESHOT | EPOLLRDHUP;  //设置关闭连接，并且设置同一事件不要多次通知，仅仅使用单个线程处理这个事件，防止惊群
    switch (trigMode)       //配置触发模式，默认是水平触发（条件触发）也就是这里的0，EPOLLET: 边缘触发
    {
    case 0:
        break;
    case 1:
        connEvent |= EPOLLET;
        break;
    case 2:
        listenEvent |= EPOLLET;
        break;
    case 3:
        connEvent |= EPOLLET;
        listenEvent |= EPOLLET;
        break;
    default:
        listenEvent |= EPOLLET;
        connEvent |= EPOLLET;
        break;
    }

    HttpConn::isET = (connEvent & EPOLLET);
}

void WebServer::AddClient_(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    //设置fd映射addr
    users[fd].init(fd, addr);
    if (timeoutMS > 0)
    {
        //将该fd加入小根堆中
        timer->add(fd, timeoutMS, std::bind(&WebServer::CloseConn_, this, &users[fd]));
    }
    epoller->AddFd(fd, EPOLLIN | connEvent);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users[fd].GetFd());
}

void WebServer::DealListen_()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do
    {
        int fd = accept(listenFd, (struct sockaddr*)&addr, &len);
        if (fd <= 0) {return;}
        else if (HttpConn::userCount >= MAX_FD)
        {
            SendError_(fd, "Server busy");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    }while (listenEvent & EPOLLET);
}

void WebServer::DealWrite_(HttpConn* client)
{
    assert(client);
    ExtentTime_(client);
    //为Onwrite_绑定client参数，作为一个可调用对象传递
    threadpool->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::DealRead_(HttpConn* client)
{
    assert(client);
    ExtentTime_(client);
    threadpool->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::SendError_(int fd, const char* info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0)
    {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::ExtentTime_(HttpConn* client)
{
    assert(client);
    if (timeoutMS > 0)
    {
        timer->adjust(client->GetFd(), timeoutMS);
    }
}

void WebServer::CloseConn_(HttpConn* client)
{
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller->DelFd(client->GetFd());
    client->Close();
}

void WebServer::OnRead_(HttpConn* client)
{
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN)
    {
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnWrite_(HttpConn* client)
{
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0)
    {
        //传输完成
        if (client->IsKeepAlive())
        {
            OnProcess(client);
            return;
        }
    }
    else if (ret < 0)
    {
        if (writeErrno == EAGAIN)
        {
            //继续传输
            epoller->ModFd(client->GetFd(), connEvent | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

void WebServer::OnProcess(HttpConn* client)
{
    if (client->process())
    {
        epoller->ModFd(client->GetFd(), connEvent | EPOLLOUT);
    }
    else
    {
        epoller->ModFd(client->GetFd(), connEvent | EPOLLIN);
    }
}

int WebServer::SetFdNonblock(int fd)
{
    //将文件描述符设置为非阻塞模式
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
