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
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "server.h"
#include "isc_msg.h"

/** G L O B A L  V A R I A B L E S **********************************/
int pid = -1;
std::atomic<bool> quit(false);

/** P R I V A T E  F U N C T I O N S ********************************/
static void sig_handler(int sig)
{
    quit.store(true);
}

/*
 * main program entry
 */
int main(int argc, char* argv[])
{
    int port = BASE_PORT;
    int numConns = 999;
    int opt;

    while ((opt = getopt(argc, argv, ":p:n:h")) != -1)
    {
        switch (opt)
        {
        default:
        case '?':
            fprintf(stderr, "unknown option: %c\n", optopt);
        case 'h':
            fprintf(stdout, "-p for server port number\n"
                            "-n for maximum number of clients\n");
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'n':
            numConns = atoi(optarg);
            break;
        case ':':
            fprintf(stderr, "option needs a value\n");
            break;
        }
    }

    Message message;
    //printf("sizeof message: %d\n", sizeof(isc_msg_t));
    std::unique_ptr<ServerBase> server = nullptr;
    signal(SIGINT, sig_handler); // register signal handler

    // split into 2 processes
    pid = fork();
    if (pid > 0) // parent process
    {
        server = make_unique_cpp11<Switch>(port, numConns);
        server->run();
    }
    else if (pid == 0) // child process
    {
        server = make_unique_cpp11<Logger>();
        server->run();
    }
    else // error
    {
        perror("fork() failed");
        return 1;
    }

    while (server->isRunning())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (quit.load())
            break; // exit normally after SIGINT
    }

    if (pid > 0) // parent process
        wait(nullptr);
    return 0;
}
