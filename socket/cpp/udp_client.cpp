/*
 * references
 *  - https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/
 *  - https://www.geeksforgeeks.org/udp-server-client-implementation-c/
 *
 */

#include <netdb.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <exception>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <cctype>
#include <chrono>
#include <thread>
#include <getopt.h>
#include <netinet/tcp.h>

#include <queue>
#include <vector>

#include "logging.hpp"
#include "config.hpp"

static struct option long_options[] = {
    {"help",                no_argument,        nullptr,  'h' },
    {"server-hostname",     required_argument,  nullptr,  's' },
    {"port",                required_argument,  nullptr,  'p' },
    {"buffer-size",         required_argument,  nullptr,  'b' },
    {"debug",               no_argument,        nullptr,  'd' },
    {nullptr,               0,                  nullptr,  0 },
};

void print_help(void) {
    LOG_INFO("usage: tcp_client [-h] [-H HOST] [-p PORT]\n\n");
    LOG_INFO("This is toy udp client to test comm latency\n\n");
    LOG_INFO("options:\n");

    LOG_INFO("  -h,      --help                  show this help message and exit\n");
    LOG_INFO("  -s HOST, --server-hostname HOST  set server hostname\n");
    LOG_INFO("  -p PORT, --port            PORT  set server port number\n");
    LOG_INFO("  -b SIZE, --buffer-size     SIZE  set message buffer-size\n");
    LOG_INFO("  -d,      --debug                 enable debug messages\n");
}

int func(int sockfd, const struct sockaddr_in &servaddr, size_t buffer_size,  bool debug)
{
    debug = debug;
    int ret = EXIT_SUCCESS;
    std::priority_queue<double, std::vector<double>, std::greater<double>> min_heap;

    char *buff = (char*) std::malloc(buffer_size);
    std::memcpy(buff, MESSAGE_BEGIN_STRING, sizeof(MESSAGE_BEGIN_STRING) - 1);
    std::memcpy(buff + buffer_size - sizeof(MESSAGE_END_STRING) - 1 , MESSAGE_END_STRING, sizeof(MESSAGE_END_STRING) - 1);

    // int n;
    uint64_t count = 0;
    auto b = std::chrono::high_resolution_clock::now();
    for (;;) {
        auto b_frame = std::chrono::high_resolution_clock::now();
        //bzero(buff, sizeof(buff));
        //LOG_INFO("Enter the string : ");
        // n = 0;
        // while ((buff[n++] = getchar()) != '\n') {
        //     LOG_INFO("<%c>\n", buff[n-1]);
        // }
        // buff[MAX-1] = count;
        count++;
        if (sendto(sockfd, buff, buffer_size, UDP_SEND_FLAG, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            LOG_ERR("_send failed: %s\n", strerror(errno));
            break;
        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::memset(buff, 0x0, buffer_size);

        socklen_t len;
        if (recvfrom(sockfd, buff, buffer_size, UDP_RECV_FLAG, (struct sockaddr *)&servaddr, &len) <= 0) {
            LOG_ERR("_recv failed: %s\n", strerror(errno));
            LOG_INFO("Client exit...\n");
            break;
        }
        // LOG_INFO("From Server : %s(%d)", buff, buff[MAX-1]);
        if (strncmp(buff, MESSAGE_BEGIN_STRING, sizeof(MESSAGE_BEGIN_STRING) - 1) != 0) {
            LOG_ERR("message is broken!\n");
            ret = EXIT_FAILURE;
            break;
        }

        if (strncmp(buff + buffer_size - sizeof(MESSAGE_END_STRING) - 1, MESSAGE_END_STRING, sizeof(MESSAGE_END_STRING) - 1) != 0) {
            LOG_ERR("message is broken!\n");
            ret = EXIT_FAILURE;
            break;
        }

        auto e_frame = std::chrono::high_resolution_clock::now();
        double elapsed_frame = std::chrono::duration<double, std::milli>(e_frame - b_frame).count();

        if (min_heap.size() < 10) {
            min_heap.push(elapsed_frame);
        } else {
            if (elapsed_frame > min_heap.top()) {
                min_heap.pop();
                min_heap.push(elapsed_frame);
            }
        }

        // if (elapsed_frame > 30.0) LOG_INFO(" - %f milliseconds\n", elapsed_frame);

        auto e = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(e - b).count();
        if (elapsed > PERF_MONITOR_INTERVAL) {
            float fps = 1000.0*count/elapsed;
            LOG_INFO("\t--------------------------------------------------------------------------------\n");
            LOG_INFO("\t    fps: %6f hz\telapsed= %9f milliseconds/ %ld\n", fps, elapsed, count);
            LOG_INFO("\t    average elapsed time per frame: %3.3f millisecs\n", elapsed/count);
            LOG_INFO("\t    [");
            while (!min_heap.empty()) {
                std::printf("%.3f, ", min_heap.top());
                min_heap.pop();
            }
            std::printf("]\n");
            LOG_INFO("\t--------------------------------------------------------------------------------\n");
            //reset
            b = std::chrono::high_resolution_clock::now();
            count = 0;
        }
    }

    std::free(buff);
    return ret;
}

class InterruptException : public std::exception
{
public:
    InterruptException(int s) : S(s) {}
    int S;
};

void signal_handler(int s)
{
    throw InterruptException(s);
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in cliaddr;
    struct sockaddr_in servaddr;

    // init signal handler
    std::signal(SIGINT, signal_handler);

    //
    // reference
    //  - https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
    //  - https://man7.org/linux/man-pages/man3/getopt.3.html
    //
    const char *server_hostname_default = "127.0.0.1";
    char *server_hostname = (char *)server_hostname_default;
    int port            = PORT;
    size_t buffer_size  = MAX_BUFFER_SIZE;
    bool debug = false;
    int c;
    opterr = 0;

    int option_index = 0;
    while ((c = getopt_long (argc, argv, "hs:p:b:d", long_options, &option_index)) != -1)
        switch (c)
        {
            case 0:
                LOG_INFO("option %s", long_options[option_index].name);
                if (optarg)
                       LOG_INFO(" with arg %s", optarg);
                LOG_INFO("\n");
                break;
            case 'h':
                print_help();
                return 0;
            case 's':
                server_hostname = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'b':
                buffer_size = (size_t)atoi(optarg);
                break;
            case 'd':
                debug = true;
                break;
            case '?':
                if (optopt == 'r')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (std::isprint (optopt))
                    fprintf (stderr, "Unknown option or no argument `-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                LOG_INFO("return=%c(%x)\n", c, c);
                abort ();
        }

    // LOG_INFO("optind=%d\n", optind);

    LOG_INFO("\tserver_hostname: %s\n", server_hostname);
    LOG_INFO("\tport           : %d\n", port);
    LOG_INFO("\tbuffer-size    : %ld\n", buffer_size);

    if (buffer_size < 64) {
        LOG_INFO("buffer-size is too\n");
        std::exit(EXIT_FAILURE);
    }

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        LOG_INFO("socket creation failed...\n");
        std::exit(EXIT_FAILURE);
    }
    else
        LOG_INFO("Socket successfully created..\n");

    // assign IP, PORT
    std::memset(&cliaddr, 0x0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    cliaddr.sin_port = htons(port + 1);

    if ((bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr))) != 0) {
        LOG_ERR("socket bind failed...\n");
        close(sockfd);
        std::exit(EXIT_FAILURE);
    }
    else
        LOG_INFO("Socket successfully binded..\n");

    // int one = 1;
    // int zero = 0;
    // if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0) {
    //     LOG_ERR("setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) failed: %s\n", strerror(errno));
    //     close(sockfd);
    //     std::exit(EXIT_FAILURE);
    // }

    // if (setsockopt(sockfd, SOL_TCP, TCP_CORK, &zero, sizeof(zero)) < 0) {
    //     LOG_ERR("setsockopt(sockfd, SOL_TCP, TCP_CORK, &zero, sizeof(zero)) failed: %s\n", strerror(errno));
    //     close(sockfd);
    //     std::exit(EXIT_FAILURE);
    // }

    int retbuf = 0;
    socklen_t optlen;
    optlen = sizeof(retbuf);
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &retbuf, &optlen) < 0) {
        LOG_ERR("getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &retbuf, &optlen) failed: %s(%d)\n", strerror(errno), errno);
        close(sockfd);
        std::exit(EXIT_FAILURE);
    }
    LOG_INFO("SNDBUF send buffer size = %d\n", retbuf);

    optlen = sizeof(retbuf);
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &retbuf, &optlen) < 0) {
        LOG_ERR("getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &retbuf, &optlen) failed: %s(%d)\n", strerror(errno), errno);
        close(sockfd);
        std::exit(EXIT_FAILURE);
    }
    LOG_INFO("RCVBUF recv buffer size = %d\n", retbuf);

    // assign IP, PORT
    std::memset(&servaddr, 0x0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(server_hostname);
    servaddr.sin_port = htons(port);

    int ret = EXIT_SUCCESS;

    // // connect the client socket to server socket
    // if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
    //     LOG_ERR("connect(): connection with the server failed, %s(%d)\n", strerror(errno), errno);
    //     close(sockfd);
    //     std::exit(EXIT_FAILURE);
    // }
    // else
    //     LOG_INFO("connected to the server..\n");

    //get host name
    char hostname[16];
    memset(hostname, 0x0, sizeof(hostname));
    gethostname(hostname, sizeof(hostname));
    LOG_INFO("Device Name (=hostname): %s\n", hostname);

    // reference
    //  - https://stackoverflow.com/questions/49335001/get-local-ip-address-in-c
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    if (getsockname(sockfd, (struct sockaddr*)&name, &namelen) < 0) {
        LOG_ERR("getsockname(): failed, %s(%d)\n", strerror(errno), errno);
        std::exit(EXIT_FAILURE);
    }

    char buffer[INET_ADDRSTRLEN];
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, INET_ADDRSTRLEN);
    if(p != nullptr) {
        LOG_INFO("Local IP address is: %s\n", buffer);
    } else {
        LOG_ERR("Error number: %s(%d)\n", strerror(errno), errno);
    }

    // reference
    //  * https://stackoverflow.com/questions/46484240/getpeername-from-listeningserver-socket
    // struct sockaddr_in addr1;
    struct sockaddr_in addr2;

    socklen_t serv_len = sizeof(struct sockaddr_in);
    // if (getpeername(sockfd, (struct sockaddr *)&addr1, &serv_len) < 0) {
    //     LOG_ERR("getpeername(): failed, %s(%d)\n", strerror(errno), errno);
    //     std::exit(EXIT_FAILURE);
    // }
    if (getsockname(sockfd, (struct sockaddr *)&addr2, &serv_len) < 0) {
        LOG_ERR("getsockname(): failed, %s(%d)\n", strerror(errno), errno);
        std::exit(EXIT_FAILURE);
    }

    LOG_INFO("Client socket's ip : %s\n", inet_ntoa(addr2.sin_addr));
    LOG_INFO("client socket's port %d\n", ntohs(addr2.sin_port));
    // LOG_INFO("Client socket's peer ip : %s\n", inet_ntoa(addr1.sin_addr));
    // LOG_INFO("Client socket's peer port : %d\n", ntohs(addr1.sin_port));

    // LOG_INFO("client socket error=%d peer_error=%d\n", clnt_sock_err, clnt_peer_err);

    // function for chat
    try {
        ret = func(sockfd, servaddr, buffer_size, debug);
    } catch (InterruptException &e) {
        LOG_ERR("Terminated by Interrupt %s\n", e.what());
    }

    // close the socket
    close(sockfd);

    return ret;
}
