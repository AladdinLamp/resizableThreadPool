#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    ThreadPool(size_t);

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;

    void stopThreads();

    int  resize(int threads); 

    int  getThreadNum();

    ~ThreadPool();
private:
    void newWorker(int i);

    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;
    std::vector< bool > flags;
    
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
    int  m_threadNum;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false)
{
    m_threadNum = threads;

    std::vector< bool > tmp(m_threadNum, false);
    flags.swap(tmp);

    for(size_t i = 0;i<threads;++i)
        newWorker(i);
}

void ThreadPool::newWorker(int i)
{
    workers.emplace_back(
        [this, i]
        {
            for(;;)
            {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock,
                        [this,i]{ return this->stop || this->flags[i] || !this->tasks.empty(); });
                    if (this->flags[i]) 
                        return;
                    if(this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                task();
            }
        }
    );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });

    }
    condition.notify_one();


    return res;
}

void ThreadPool::stopThreads()
{
    stop = true;
}

int ThreadPool::getThreadNum()
{
    return m_threadNum;
}

int ThreadPool::resize(int threads)
{

    if (threads == 0) 
        return -1;
    if (threads == m_threadNum) 
        return 0;

    //worker removes
    std::vector< int > removes;

    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);

        if (threads > m_threadNum) { 

            std::vector< bool > tmp(m_threadNum, false);
            flags.swap(tmp);
            for (int i = m_threadNum; i < threads; i++) { 
                newWorker(i);
            }

        } else { 
            for (int i = threads; i < m_threadNum; i++) { 
                flags[i] = true;
                removes.push_back(i);
            }
        }
        m_threadNum = threads;
    }

    //remove thread
    std::thread th;
    if (!removes.empty()) { 
        condition.notify_all();
        for (int i = removes.size() - 1; i >= 0 ; i--) { 
            int j = removes[i];
            workers[j].join();
            workers.erase(workers.begin() + j);
        }
        removes.clear();

        std::vector< bool > tmp(m_threadNum, false);
        flags.swap(tmp);
    }
    
    return 0;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}

#endif
