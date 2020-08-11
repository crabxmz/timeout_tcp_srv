#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <list>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace my_srv
{
    struct clientTimer
    {
        using func_ptr = std::function<void(struct clientTimer *)>;
        struct sockaddr_in adr;
        int sock_fd;
        time_t expire_ts;
        func_ptr cb_func; // defined by external modu
        clientTimer(time_t ts, func_ptr f, int fd, const struct sockaddr_in &_adr) : expire_ts(ts), cb_func(f), sock_fd(fd), adr(_adr) {}
    };
    class timerQueue
    {
        using func_ptr = std::function<void(clientTimer *)>;
        std::list<clientTimer *> _q;
        std::unordered_map<int, std::list<clientTimer *>::iterator> _umap;
        std::mutex _m;
        int TIMESLOT;

    public:
        timerQueue(int _timeslot) : TIMESLOT(_timeslot) {}
        void add_timer(const clientTimer &cli_data);
        void adjust_timer(int fd, time_t new_ts);
        void del_timer(int fd);
        void start();
        void tick();
        int get_sz()
        {
            return _q.size();
        }
        ~timerQueue();
    };
} // namespace my_srv