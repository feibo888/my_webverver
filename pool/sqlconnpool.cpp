//
// Created by fb on 12/25/24.
//


//这里semID没有初始化

#include "sqlconnpool.h"

#include <cassert>
using namespace std;

SqlConnPool* SqlConnPool::Instance()
{
    static SqlConnPool instance;
    return &instance;
}

MYSQL* SqlConnPool::GetConn()
{
    MYSQL *sql = nullptr;
    if (connQue.empty())
    {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semID);

    {
        lock_guard<mutex> locker(this->mtx);
        sql = connQue.front();
        connQue.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql)
{
    assert(sql);
    {
        lock_guard<mutex> locker(this->mtx);
        connQue.push(sql);
        sem_post(&semID);
    }
}

int SqlConnPool::GetFreeConnCount()
{
    {
        lock_guard<mutex> locker(this->mtx);
        return static_cast<int>(connQue.size());
    }
}

void SqlConnPool::init(const char* host, int port,
                    const char* user, const char* passwd,
                    const char* dbName, int connSize = 10)
{
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++)
    {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (!sql)
        {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, user, passwd, dbName, port, nullptr, 0);
        if (!sql)
        {
            LOG_ERROR("MySql Connect error!");
        }
        connQue.push(sql);
    }
    this->MAX_CONN = connSize;
    sem_init(&this->semID, 0, MAX_CONN);

}

void SqlConnPool::ClosePool()
{
    {
        lock_guard<mutex> locker(this->mtx);
        while (!connQue.empty())
        {
            auto item = connQue.front();
            connQue.pop();
            mysql_close(item);
        }
        mysql_library_end();
    }
}

SqlConnPool::SqlConnPool()
{
    this->useCount = 0;
    this->freeCount = 0;
}

SqlConnPool::~SqlConnPool()
{
    this->ClosePool();
}
