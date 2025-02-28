//
// Created by fb on 12/25/24.
//

#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"


class SqlConnPool
{
public:
    static SqlConnPool* Instance();

    MYSQL* GetConn();
    void FreeConn(MYSQL* conn);
    int GetFreeConnCount();

    void init(const char* host, int port,
        const char* user, const char* passwd,
        const char* dbName, int connSize);

    void ClosePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN;
    int useCount;
    int freeCount;

    std::queue<MYSQL*> connQue;
    std::mutex mtx;
    sem_t semID;


};




#endif //SQLCONNPOOL_H
