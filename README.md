# WebServer

## 简介

用C++实现的高性能WEB服务器，经过webbenchh压力测试可以实现上万的QPS

- 利用IO复用技术Epoll与线程池实现多线程的Reactor高并发模型；
- 利用正则与状态机解析HTTP请求报文，实现处理静态资源的请求；
- 利用标准库容器封装char，实现自动增长的缓冲区；
- 基于小根堆实现的定时器，关闭超时的非活动连接；
- 利用单例模式与阻塞队列实现异步的日志系统，记录服务器运行状态；
- 利用RAII机制实现了数据库连接池，减少数据库连接建立与关闭的开销，同时实现了用户注册登录功能。
- 增加logsys,threadpool测试单元(todo: timer, sqlconnpool, httprequest, httpresponse)
- 使用boost正则
- 可由docker启动

## 启动

安装好docker后，在项目根目录下可直接编译启动

```dockerfile
docker-compose up -d
```

## 测试

使用webbench进行压测

```shell
./webbench-1.5/webbench -c 100 -t 10 http://ip:port/
./webbench-1.5/webbench -c 1000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 5000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 10000 -t 10 http://ip:port/
```

- 测试环境:wsl2(ubuntu20 ) CPU：i5-12500H 内存：16G

- QPS：60000+

## 致谢

[markparticle/WebServer: C++ Linux WebServer服务器](https://github.com/markparticle/WebServer?tab=readme-ov-file)

[Time-Zero/TinyWebServer: 一个C++11实现的TinyWebServer](https://github.com/Time-Zero/TinyWebServer)

