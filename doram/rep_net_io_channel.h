#pragma once

#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <string>

#include "emp-tool/io/io_channel.h"
using namespace std;

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace emp
{
double time_total_network = 0;
class RepSubChannel
{
  public:
    int sock;
    FILE *stream = nullptr;
    char *buf = nullptr;
    int ptr;
    char *stream_buf = nullptr;
    uint64_t counter = 0;
    uint64_t flushes = 0;
    RepSubChannel(int sock) : sock(sock)
    {
        // stream_buf = new char[NETWORK_BUFFER_SIZE];
        // buf = new char[NETWORK_BUFFER_SIZE2];
        // stream = fdopen(sock, "wb+");
        // memset(stream_buf, 0, NETWORK_BUFFER_SIZE);
        // setvbuf(stream, stream_buf, _IOFBF, NETWORK_BUFFER_SIZE);
    }
    ~RepSubChannel()
    {
        // fclose(stream);
        // delete[] stream_buf;
        // delete[] buf;
    }
};

class RepSenderSubChannel : public RepSubChannel
{
  public:
    RepSenderSubChannel(int sock) : RepSubChannel(sock)
    {
        ptr = 0;
        // fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
        //  setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));
    }

    void flush()
    {
        flushes++;
        // send_data_raw(buf, ptr);
        // round up to the next NBS2
        // if (counter % NETWORK_BUFFER_SIZE2 != 0)
        // send_data_raw(buf + ptr, NETWORK_BUFFER_SIZE2 - counter % NETWORK_BUFFER_SIZE2);
        // fflush(stream);
        ptr = 0;
    }

    void send_data(const void *data, int len)
    {
        send_data_raw(data, len);
        /*
        if (len <= NETWORK_BUFFER_SIZE2 - ptr) {
            memcpy(buf + ptr, data, len);
            ptr += len;
        } else {
            send_data_raw(buf, ptr);
            send_data_raw(data, len);
            ptr = 0;
        } */
    }

    void send_data_raw(const void *data, int len)
    {
        counter += len;
        int sent = 0;
        while (sent < len)
        {
            // int res = fwrite(sent + (char *)data, 1, len - sent, stream);
            int res = write(sock, sent + (char *)data, len);
            if (res >= 0)
                sent += res;
            // else //!we wait here, like, a lot.
            // fprintf(stderr, "error: send_data_raw %d %d\n", res, errno);
        }
    }
};

class RepRecverSubChannel : public RepSubChannel
{
  public:
    pollfd poll_sock;
    RepRecverSubChannel(int sock) : RepSubChannel(sock)
    {
        ptr = NETWORK_BUFFER_SIZE2;
        poll_sock.fd = sock;
        poll_sock.events = POLLIN;
        // setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
        //  fcntl(sock, F_SETFL, O_NONBLOCK);
    }

    void flush()
    {
        flushes++;
        ptr = NETWORK_BUFFER_SIZE2;
    }

    void recv_data(void *data, int len)
    {
        recv_data_raw(data, len);
        /*
        if (len <= NETWORK_BUFFER_SIZE2 - ptr) {
            memcpy(data, buf + ptr, len);
            ptr += len;
        } else {
            int remain = len;
            memcpy(data, buf + ptr, NETWORK_BUFFER_SIZE2 - ptr);
            remain -= NETWORK_BUFFER_SIZE2 - ptr;
            while (true) {
                recv_data_raw(buf, NETWORK_BUFFER_SIZE2);
                if (remain <= NETWORK_BUFFER_SIZE2) {
                    memcpy(len - remain + (char *)data, buf, remain);
                    ptr = remain;
                    break;
                } else {
                    memcpy(len - remain + (char *)data, buf, NETWORK_BUFFER_SIZE2);
                    remain -= NETWORK_BUFFER_SIZE2;
                }
            }
        } */
    }

    void recv_data_raw(void *data, int len)
    {
        counter += len;
        int sent = 0;
        do
        {
            // int res = fread(sent + (char *)data, 1, len - sent, stream);
            int res = read(sock, sent + (char *)data, len - sent);
            if (res >= 0)
                sent += res;
        } while (sent < len /*&& poll(&poll_sock, 1, -1)*/);
    }
};

class RepNetIO : public IOChannel<RepNetIO>
{
  public:
    bool is_server, quiet;
    int send_sock = 0;
    int recv_sock = 0;
    int FSM = 0;
    double time_in_send = 0;
    double time_in_recv = 0;
    double time_in_flush = 0;
    RepSenderSubChannel *schannel;
    RepRecverSubChannel *rchannel;

    RepNetIO(const char *address, int send_port, bool quiet = false) : quiet(quiet)
    {
        int recv_port = send_port + 50; // needs to be between NUM_THREADS and 100
        if (send_port < 0 || send_port > 65535)
        {
            throw std::runtime_error("Invalid send port number!");
        }
        if (recv_port < 0 || recv_port > 65535)
        {
            throw std::runtime_error("Invalid receive port number!");
        }

        is_server = (address == nullptr);
        if (is_server)
        {
            recv_sock = server_listen(send_port);
            usleep(2000);
            send_sock = server_listen(recv_port);
        }
        else
        {
            send_sock = client_connect(address, send_port);
            recv_sock = client_connect(address, recv_port);
        }
        FSM = 0;
        set_delay_opt(send_sock, true);
        set_delay_opt(recv_sock, true);
        schannel = new RepSenderSubChannel(send_sock);
        rchannel = new RepRecverSubChannel(recv_sock);
        if (!quiet) cout << "connected" << endl;
    }

    int server_listen(int port)
    {
        int mysocket;
        struct sockaddr_in dest;
        struct sockaddr_in serv;
        socklen_t socksize = sizeof(struct sockaddr_in);
        memset(&serv, 0, sizeof(serv));
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr = htonl(INADDR_ANY); /* set our address to any interface */
        serv.sin_port = htons(port);              /* set the server port number */
        mysocket = socket(AF_INET, SOCK_STREAM, 0);
        int reuse = 1;
        setsockopt(mysocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
        if (bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr)) < 0)
        {
            perror("error: bind");
            exit(1);
        }
        if (listen(mysocket, 1) < 0)
        {
            perror("error: listen");
            exit(1);
        }
        int sock = accept(mysocket, (struct sockaddr *)&dest, &socksize);
        close(mysocket);
        return sock;
    }
    int client_connect(const char *address, int port)
    {
        int sock;
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;

        if (!quiet) cout << "trying to determine inet_addr" << endl;

        dest.sin_addr.s_addr = inet_addr(address);
        dest.sin_port = htons(port);

        if (!quiet) cout << "entering connect loop on " << address << ":" << port << endl;

        while (1)
        {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (connect(sock, (struct sockaddr *)&dest, sizeof(struct sockaddr)) == 0)
                break;

            close(sock);
            usleep(1000);
        }

        if (!quiet) cout << "connected as client on " << port << endl;

        return sock;
    }

    ~RepNetIO()
    {
        flush();
        delete schannel;
        delete rchannel;
        close(send_sock);
        close(recv_sock);
    }

    void sync()
    {
    }

    void set_delay_opt(int sock, bool enable_nodelay)
    {
        if (enable_nodelay)
        {
            const int one = 1;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        }
        else
        {
            const int zero = 0;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &zero, sizeof(zero));
        }
    }

    // so they do this FSM thing to avoid deadlock
    // state 0: starting state
    // state 1: I blocked waiting to receive but now I actually got something
    // state 2: run happily until I need to recv, at that point actually send

    void flush()
    {
        auto _start = clock_start();
        schannel->flush();
        time_total_network += time_from(_start);
    }

    void send_data_internal(const void *data, int len)
    {
        auto _start = clock_start();
        schannel->send_data(data, len);
        double time_spent = time_from(_start);
        time_in_send += time_spent;
        time_total_network += time_spent;
    }

    void recv_data_internal(void *data, int len)
    {
        auto _start = clock_start();
        rchannel->recv_data(data, len);
        double time_spent = time_from(_start);
        time_in_recv += time_spent;
        time_total_network += time_spent;
    }
};

} // namespace emp
