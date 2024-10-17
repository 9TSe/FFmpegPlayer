#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <queue>
#include <vector>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>

class ForbidCopy
{
public:
    ~ForbidCopy() = default;
protected:
    ForbidCopy() = default;
private:
    ForbidCopy(const ForbidCopy&) = delete;
    ForbidCopy& operator=(const ForbidCopy&) = delete;
};


class ThreadPool : public ForbidCopy
{
public:
    using Task = std::packaged_task<void()>;
    static ThreadPool& instance()
    {
        static ThreadPool ins;
        return ins;
    }
    ~ThreadPool()
    {
        stop();
    }

    template<class F, typename... Arg>
    auto commitTask(F&& funcName, Arg&&... args)
        ->std::future<decltype(std::forward<F>(funcName)(std::forward<Arg>(args)...))>
    {
        using returnType = decltype(std::forward<F>(funcName)(std::forward<Arg>(args)...));
        if(m_stop.load()) return std::future<returnType>();
        auto task = std::make_shared<std::packaged_task<returnType()>>(std::bind(
            std::forward<F>(funcName), std::forward<Arg>(args)...));

        std::future<returnType> ret = task->get_future();
        {
            std::lock_guard<std::mutex> mutex(m_mutex);
            m_tasks.emplace([task]{(*task)();});
        }
        m_cv.notify_one();
        return ret;
    }


private:
    ThreadPool(uint16_t threadnum = std::thread::hardware_concurrency())
        :m_stop(false)
    {
        m_threadNum = threadnum <= 6 ? 6 : threadnum;

        for(int i = 0; i < threadnum; ++i){
            m_pool.emplace_back([this](){
                while(!m_stop.load()){
                    Task task;
                    {
                        std::unique_lock<std::mutex> mutex(m_mutex);
                        m_cv.wait(mutex, [this](){
                            return m_stop.load() || !m_tasks.empty();
                        });
                        if(m_tasks.empty()) return;
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                    m_threadNum--;
                    task();
                    m_threadNum++;
                }
            });
        }
    }
    void stop()
    {
        m_stop.store(true);
        m_cv.notify_all();
        for(std::thread& thread : m_pool)
        {
            if(thread.joinable()){
                thread.join();
            }
        }
    }


private:
    std::condition_variable m_cv;
    std::mutex m_mutex;
    std::atomic_bool m_stop;
    std::atomic_int m_threadNum;
    std::vector<std::thread> m_pool;
    std::queue<Task> m_tasks;

};

#endif // THREADPOOL_H
