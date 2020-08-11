#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
namespace my_srv
{
    class threadPool
    {
        std::mutex _m;
        std::condition_variable _cv;
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> task_q;
        std::atomic<int> _idl_cnt;
        bool stopped;
        size_t MAX_THREAD_NUM;

    public:
        threadPool(size_t max_thread_num) : MAX_THREAD_NUM(max_thread_num), _idl_cnt(max_thread_num), stopped(false)
        {
            for (size_t i = 0; i < max_thread_num; i++)
            {
                workers.emplace_back(
                    [this] {
                        for (;;)
                        {
                            std::function<void()> task;

                            {
                                std::unique_lock<std::mutex> lk(this->_m);
                                this->_cv.wait(lk,
                                    [this] { return this->stopped || !this->task_q.empty(); });
                                if (this->stopped && this->task_q.empty())
                                    return;
                                task = std::move(this->task_q.front());
                                this->task_q.pop();
                            }
                            _idl_cnt--;
                            task();
                            _idl_cnt++;
                        }
                    });
            }
        }
        template <class F, class... Args>
        void enqueue(F &&f, Args &&... args)
        {
            {
                std::unique_lock<std::mutex> lk(_m);
                if (stopped)
                {
                    std::runtime_error("enqueue on stopped threadpool");
                }
                task_q.emplace(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            }
            _cv.notify_one();
        }
        int get_idle_thread_num()
        {
            return _idl_cnt;
        }
        ~threadPool()
        {
            {
                std::unique_lock<std::mutex> lk(_m);
                stopped = true;
            }
            _cv.notify_all();
            for (auto &t : workers)
            {
                if (t.joinable())
                    t.join();
            }
        }
    };
} // namespace my_srv