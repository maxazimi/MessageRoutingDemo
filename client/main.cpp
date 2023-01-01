/**
 * ISC Challenge project.
 *
 * Author: Mehdi Azimi
 *
 * Compile and run the project on a POSIX-compliance platform
 * e.g. Linux, macOS etc.
 *
 * License:
 * YOU ARR FREE TO USE, MODIFY OR DISTRIBUTE the source code
 * unconditionally.
 */
#include <iostream>
#include <signal.h>
#include "client.h"

/** G L O B A L  V A R I A B L E S **********************************/
std::atomic<bool> quit(false);

/** P R I V A T E  F U N C T I O N S ********************************/
static bool getInputMsg(int& msg, int& dstId)
{
    fd_set read_fds;
    struct timeval tv;

    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms

    int result = select(1, &read_fds, nullptr, nullptr, &tv);
    if (result >= 0)
    {
        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            bool ret = true;
            char buf[1024];
            read(STDIN_FILENO, buf, sizeof(buf));

            if (buf[0] >= '0' && buf[0] <= '9')
                sscanf(buf, "%d %d", &msg, &dstId);
            else
                ret = false;
            
            buf[0] = 0; // clear buffer
            return ret;
        }
    }
    else if (result == -1 && errno == EINTR) // interrupt signal received
    {
        quit.store(true);
    }

    return false;
}

/*
 * main program entry
 */
int main(int argc, char* argv[])
{
    std::string ipAddress = "127.0.0.1";
    int port = BASE_PORT;
    int srcId = 0;
    int dstId = 0;
    int msg = 0;
    int opt;

    while ((opt = getopt(argc, argv, ":a:p:s:h")) != -1)
    {
        switch (opt)
        {
        default:
        case '?':
            fprintf(stderr, "unknown option: %c\n", optopt);
        case 'h':
            fprintf(stdout, "-a for server IP address\n"
                            "-p for server port number\n"
                            "-s for source ID (from 1 to 999)\n");
            exit(0);
        case ':':
            fprintf(stderr, "option needs a value\n");
            exit(0);
        case 'a':
            ipAddress = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 's':
            srcId = atoi(optarg);
            break;
        }
    }

    Member member(ipAddress, port, srcId);

    while (member.isRunning())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (quit.load())
            break; // exit normally after SIGINT

        if (getInputMsg(msg, dstId))
        {
            member.sendMessage(msg, dstId);
        }
    }
    return 0;
}
