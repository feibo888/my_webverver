cmake_minimum_required(VERSION 3.16.3)
project(my_webServer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")  # 添加 -O3 优化

include_directories(/usr/include/mysql)
#find_package(MySQL REQUIRED)
#include_directories(${MYSQL_INCLUDE_DIRS})

add_executable(my_webServer main.cpp
        server/webserver.h
        server/webserver.cpp
        server/epoller.h
        server/epoller.cpp
        pool/sqlconnpool.h
        pool/sqlconnpool.cpp
        log/log.h
        pool/sqlconnRAll.h
        pool/threadpool.h
        http/httpconn.h
        http/httpconn.cpp
        http/httprequest.h
        http/httprequest.cpp
        http/httpresponse.h
        http/httpresponse.cpp
        buffer/buffer.h
        timer/heaptimer.h
        timer/heaptimer.cpp
        buffer/buffer.cpp
        log/blockqueue.h
        log/log.cpp
)


target_link_libraries(my_webServer mysqlclient pthread)
