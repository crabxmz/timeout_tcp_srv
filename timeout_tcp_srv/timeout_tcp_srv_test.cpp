#include "timeout_tcp_srv.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("please specify a port to listen");
        return -1;
    }
    my_srv::epollSigServer test_srv(atoi(argv[1]));
    test_srv.event_listener(my_srv::epollSigServer::MAX_LISTEN_SOCK);
    test_srv.event_loop();
    return 0;
}
