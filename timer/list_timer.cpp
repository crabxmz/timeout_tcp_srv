#include "list_timer.h"

namespace my_srv
{
    void timerQueue::start()
    {
        alarm(TIMESLOT);
    }
    void timerQueue::tick()
    {
        time_t cur = time(0);
        while (!_q.empty() && _q.front()->expire_ts < cur)
        {
            del_timer(_q.front()->sock_fd);
        }
        alarm(TIMESLOT);
    }
    void timerQueue::add_timer(const clientTimer &cli_data)
    {
        std::lock_guard<std::mutex> lk(_m);
        clientTimer *x = new clientTimer(cli_data);
        auto it = _q.begin();
        while (it != _q.end() && (*it)->expire_ts <= cli_data.expire_ts)
        {
            it++;
        }
        _umap.emplace(cli_data.sock_fd, _q.insert(it, x));
    }
    void timerQueue::del_timer(int fd)
    {
        std::lock_guard<std::mutex> lk(_m);
        std::list<clientTimer *>::iterator it = _umap.at(fd);
        clientTimer *p = *it;
        _q.erase(it);
        _umap.erase(fd);
        if (p)
        {
            p->cb_func(p);
            delete p;
        }
    }
    void timerQueue::adjust_timer(int fd, time_t new_ts)
    {
        std::lock_guard<std::mutex> lk(_m);
        clientTimer *x = *(_umap.at(fd));
        _q.erase(_umap.at(fd));
        x->expire_ts = new_ts;
        auto it = _q.begin();
        while (it != _q.end() && (*it)->expire_ts <= new_ts)
        {
            it++;
        }
        _umap[fd] = _q.insert(it, x);
    }
    timerQueue::~timerQueue()
    {
        while (!_q.empty())
        {
            auto p = _q.front();
            _q.pop_front();
            if (p)
            {
                p->cb_func(p);
                delete p;
            }
        }
        _umap.clear();
    }
} // namespace my_srv