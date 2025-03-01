//
// Created by fb on 12/25/24.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>


class ThreadPool
{
public:
    explicit ThreadPool(size_t threadCount = 8):pool_(std::make_shared<Pool>())
    {
        assert(threadCount > 0);
        for (size_t i = 0; i < threadCount; ++i)
        {   
            //按值访问pool,线程数量为8时，共引用pool_9次
            std::thread([pool = pool_]  
            {
                std::unique_lock<std::mutex> locker(pool->mtx);
                while (true)
                {
                    if (!pool->tasks.empty())   //队列里有任务时，取出任务执行并释放锁，执行完后再尝试加锁
                    {
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();
                        task();
                        //std::cout << std::this_thread::get_id() << std::endl;
                        locker.lock();
                    }
                    else if (pool->isClosed) break;
                    else pool->cond.wait(locker);       //没任务时，阻塞等待，直到被AddTask唤醒，此时释放锁，wait必须使用unique_ptr
                }
            }).detach();
        }
    }

    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    ~ThreadPool()
    {
        if (static_cast<bool>(pool_))
        {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            pool_->cond.notify_all();
        }
    }

    //万能引用+完美转发
    template<class F>
    void AddTask(F&& task)
    {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();
    }



private:
    struct Pool
    {
        std::mutex mtx;     //保护任务队列的互斥锁
        std::condition_variable cond;   //用于线程间的任务通知
        bool isClosed;      //标志线程池是否已关闭
        std::queue<std::function<void()>> tasks;    //存储待执行任务的队列
    };
    std::shared_ptr<Pool> pool_;    //通过共享指针管理Pool，确保线程安全及生命周期
};

#endif //THREADPOOL_H
