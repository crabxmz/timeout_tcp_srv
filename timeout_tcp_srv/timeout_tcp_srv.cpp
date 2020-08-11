#include "timeout_tcp_srv.h"

namespace my_srv
{
    int epollSigServer::sig_pipe[2];
    int epollSigServer::epoll_fd;
    int epollSigServer::srv_listen_fd;
    threadPool epollSigServer::th_pool(epollSigServer::MAX_THREAD_NUM);
    timerQueue epollSigServer::tim_q(epollSigServer::TIME_SLOT);

    epollSigServer::epollSigServer(int _port)
    {
        init_epoll();
        init_sockpipe();
        init_sig();
        tim_q.start();
        stopped = false;
        ev_timeout = false;
        srv_listen_port = _port;
    }

    epollSigServer::~epollSigServer()
    {
        close(srv_listen_fd);
        close(sig_pipe[0]);
        close(sig_pipe[1]);
        close(epoll_fd);
    }

    void epollSigServer::print_err()
    {
        printf("%s:%d %s", __FILE__, __LINE__, strerror(errno));
    }
    int epollSigServer::default_srv_echo(int fd)
    {
        char test_str[] = "=====hello from server=====\n";
        strcpy(send_buf, test_str);
        int ret = send(fd, send_buf, strlen(test_str), 0);
        if (ret < 0)
        {
            print_err();
            return -1;
        }
        return ret;
    }
    void epollSigServer::setnonblocking(int fd)
    {
        int old_option = fcntl(fd, F_GETFL);
        int new_option = old_option | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_option);
    }

    void epollSigServer::addfd(int epollfd, int fd, uint32_t ev, bool enable_et)
    {
        epoll_event event;
        event.data.fd = fd;
        // event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
        ev |= EPOLLRDHUP;
        if (enable_et)
            ev|=EPOLLET;
        event.events=ev;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
        setnonblocking(fd);
    }
    void epollSigServer::delfd(int epollfd, int fd)
    {
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
    }
    void epollSigServer::modfd(int epollfd, int fd, uint32_t ev, bool enable_et)
    {
        epoll_event event;
        event.data.fd = fd;
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
        if (enable_et)
            event.events|EPOLLET;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
        setnonblocking(fd);
    }

    void epollSigServer::sig_handler(int sig)
    {
        // printf("=====handle sig=====\n");
        int save_errno = errno;
        char msg = (char)sig;
        send(sig_pipe[1], (char *)&msg, sizeof(msg), 0);
        errno = save_errno;
    }

    void epollSigServer::addsig(int sig)
    {
        struct sigaction sa ={ 0 };
        sa.sa_handler = sig_handler;
        sa.sa_flags |= SA_RESTART;
        sigfillset(&sa.sa_mask);
        int ret = sigaction(sig, &sa, NULL);
        assert(ret != -1);
    }

    int epollSigServer::event_listener(int max_listen_num)
    {
        srv_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        assert(srv_listen_fd >= 0);
        int op = 1;
        int ret = setsockopt(srv_listen_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&op, sizeof(op));
        assert(ret >= 0);

        struct sockaddr_in local_adr ={ 0 };
        local_adr.sin_family = AF_INET;
        local_adr.sin_port = htons(srv_listen_port);
        local_adr.sin_addr.s_addr = INADDR_ANY;

        ret = bind(srv_listen_fd, (struct sockaddr *)&local_adr, sizeof(local_adr));
        assert(ret >= 0);

        ret = listen(srv_listen_fd, max_listen_num);
        assert(ret >= 0);

        addfd(epoll_fd, srv_listen_fd, EPOLLIN, 1);
        return srv_listen_fd;
    }
    int epollSigServer::read_sock(int fd)
    {
        int ret = recv(fd, recv_buf, sizeof(recv_buf), 0);
        // printf("=====recv some content=====\n", ret);
        if (ret < 0)
            print_err();
        else
        {
            recv_buf[ret] = 0;
            // printf("%s\n", recv_buf);
            // printf("=====recv %d bytes=====\n", ret);
        }
        return ret;
    }
    int epollSigServer::accept_new_conn()
    {
        struct sockaddr_in cli_adr ={ 0 };
        socklen_t cli_adr_len = sizeof(cli_adr);
        int conn_fd = accept(srv_listen_fd, (struct sockaddr *)&cli_adr, &cli_adr_len);
        if (conn_fd < 0)
        {
            print_err();
            return -1;
        }
        print_adr(cli_adr, "new connection");
        clientTimer tmp(
            time(0) + TIME_SLOT,
            &epollSigServer::timeout_cb,
            conn_fd,
            cli_adr);

        tim_q.add_timer(tmp);
        return conn_fd;
    }

    void epollSigServer::print_adr(struct sockaddr_in &adr, const char *comment)
    {
        char tmp[16] ={ 0 };
        printf("=====%s: %s=====\n", inet_ntop(AF_INET, (void *)&adr.sin_addr, tmp, sizeof(tmp)), comment);
    }
    int epollSigServer::init_epoll()
    {
        epoll_fd = epoll_create(1);
        assert(epoll_fd >= 0);
        return 0;
    }

    int epollSigServer::init_sockpipe()
    {
        int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipe);
        assert(ret >= 0);
        setnonblocking(sig_pipe[1]);
        addfd(epoll_fd, sig_pipe[0], EPOLLIN, 1);
        return 0;
    }

    int epollSigServer::init_sig()
    {
        addsig(SIGHUP);
        addsig(SIGCHLD);
        addsig(SIGTERM);
        addsig(SIGINT);
        addsig(SIGALRM);
        return 0;
    }

    void epollSigServer::timeout_cb(struct clientTimer *cli_data)
    {
        assert(cli_data);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cli_data->sock_fd, 0);
        close(cli_data->sock_fd);
        print_adr(cli_data->adr, "connection closed");
        // printf("left time: %d\n", cli_data->expire_ts-time(0));
    }

    void epollSigServer::recvSig(int sig_fd)
    {
        int ret = recv(sig_pipe[0], recv_buf, sizeof(recv_buf), 0);
        if (ret == -1)
        {
            print_err();
            return;
        }
        for (int i = 0; i < ret; i++)
        {
            switch (recv_buf[i])
            {
            case SIGCHLD:
            case SIGHUP:
            {
                continue;
            }
            case SIGINT:
            case SIGTERM:
            {
                stopped = true;
                break;
            }
            case SIGALRM:
            {
                ev_timeout = true;
                break;
            }
            }
        }
    }

    void epollSigServer::readHandler(int fd)
    {
        int ret = read_sock(fd);
        if (ret < 0)
        {
            print_err();
            if (ret != EAGAIN)
            {
                tim_q.del_timer(fd);
            }
        }
        else if (ret == 0)
        {
            tim_q.del_timer(fd);
        }
        else
        {
            tim_q.adjust_timer(fd, time(0) + TIME_SLOT);
            default_srv_echo(fd);
        }
    }
    void epollSigServer::addNewConn()
    {
        int conn_fd = accept_new_conn();
        if (conn_fd >= 0)
            addfd(epoll_fd, conn_fd, EPOLLIN, 1);
    }

    int epollSigServer::event_loop()
    {
        while (!stopped)
        {
            int ready_num = epoll_wait(epoll_fd, event_arr, MAX_EVENT_NUM, TIME_SLOT*1000);
            if (ready_num < 0 && (errno != EINTR))
            {
                print_err();
                break;
            }
            for (int i = 0; i < ready_num; i++)
            {
                int sockfd = event_arr[i].data.fd;
                if (sockfd == srv_listen_fd)
                {
                    th_pool.enqueue(std::bind(&epollSigServer::addNewConn, this));
                    // modfd(epoll_fd, sockfd, EPOLLIN, 1);
                }
                else if ((sockfd == sig_pipe[0]) && (event_arr[i].events & EPOLLIN))
                {
                    th_pool.enqueue(std::bind(&epollSigServer::recvSig, this, std::placeholders::_1),
                        sockfd);
                    // modfd(epoll_fd, sockfd, EPOLLIN, 1);
                }
                // else if (event_arr[i].events & EPOLLRDHUP)
                // {
                //     delfd(epoll_fd, sockfd);
                // }
                else if (event_arr[i].events & EPOLLIN)
                {
                    th_pool.enqueue(std::bind(&epollSigServer::readHandler, this, std::placeholders::_1),
                        sockfd);
                    // modfd(epoll_fd, sockfd, EPOLLIN, 1);
                }
                else
                {
                    printf("=====something else happened=====\n");
                }
            }
            if (ev_timeout||!ready_num)
            {
                // kick off timeout-fd
                tim_q.tick();
                ev_timeout = false;
            }
            printf("active-connection: %d\n", tim_q.get_sz());
            printf("idle-thread: %d\n", th_pool.get_idle_thread_num());
        }
        printf("=====server stopped=====\n");
        return 0;
    }
} // namespace my_srv
