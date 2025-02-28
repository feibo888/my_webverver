//
// Created by fb on 12/25/24.
//

#ifndef SQLCONNRALL_H
#define SQLCONNRALL_H

#include "sqlconnpool.h"
#include <cassert>

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAll
{
public:
    SqlConnRAll(MYSQL** sql, SqlConnPool* connpool)
    {
        assert(connpool);
        *sql = connpool->GetConn();
        this->sql_ = *sql;
        connPool_ = connpool;
    }


    ~SqlConnRAll()
    {
        if (sql_)
        {
            connPool_->FreeConn(sql_);
        }
    };

private:
    MYSQL* sql_;
    SqlConnPool* connPool_;
};





#endif //SQLCONNRALL_H
