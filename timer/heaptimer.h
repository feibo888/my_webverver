//
// Created by fb on 12/28/24.
//

#ifndef HEAPTIMER_H
#define HEAPTIMER_H


#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallback;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;


struct TimerNode
{
    int id;         //用来标记定时器
    TimeStamp expires;  //设置过期时间
    TimeoutCallback cb;     //设置回调函数，将过期的http连接进行删除
    bool operator<(const TimerNode& t)
    {
        return expires < t.expires;
    }
};

class HeapTimer
{
public:
    HeapTimer(){ heap_.reserve(64); }
    ~HeapTimer(){ clear(); }

    void adjust(int id, int timeout);

    void add(int id, int timeOut, const TimeoutCallback& cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int GetNextTick();


private:
    void del_(size_t index);

    void siftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;       //存储定时器

    std::unordered_map<int, size_t> ref_;       //映射一个fd对应的定时器在heap_中的位置 key:fd  value:pos

};



#endif //HEAPTIMER_H
