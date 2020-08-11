#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/syslog.h>

/*self-defined*/
#include "../timer/list_timer.h"
#include "../threadpool/threadpool.h"

namespace my_srv
{
    class epollSigServer
    {
    public:
        epollSigServer(int _port);
        ~epollSigServer();

        int event_listener(int max_listen_num);
        int event_loop();

        constexpr static const int MAX_EVENT_NUM = 10;
        constexpr static const int MAX_BUF_SIZE = 1024;
        constexpr static const int MAX_LISTEN_SOCK = 10;
        constexpr static const int TIME_SLOT = 5;
        constexpr static const int MAX_THREAD_NUM = 10;

    private:
        static void print_adr(struct sockaddr_in &adr, const char *comment);
        static void print_err();
        /*init*/
        int init_epoll();
        int init_sockpipe();
        int init_sig();
        /*for fd*/
        void setnonblocking(int fd);
        void addfd(int epollfd, int fd, uint32_t ev, bool enable_et);
        void modfd(int epollfd, int fd, uint32_t ev, bool enable_et);
        void delfd(int epollfd, int fd);
        /*for sig*/
        static void sig_handler(int sig);
        void addsig(int sig);
        /*sock io*/
        int accept_new_conn();
        int read_sock(int fd);
        int default_srv_echo(int fd);
        /*timer*/
        static timerQueue tim_q;
        static void timeout_cb(struct clientTimer *cli_data);
        /*fd*/
        static int sig_pipe[2];
        static int epoll_fd;
        static int srv_listen_fd;
        /*port*/
        int srv_listen_port;
        /*stat*/
        bool stopped;
        bool ev_timeout;
        /*buf*/
        struct epoll_event event_arr[MAX_EVENT_NUM];
        char recv_buf[MAX_BUF_SIZE], send_buf[MAX_BUF_SIZE];
        /*thread*/
        static threadPool th_pool;
        void recvSig(int sig_fd);
        void addNewConn();
        void readHandler(int fd);
    };
} // namespace my_srv