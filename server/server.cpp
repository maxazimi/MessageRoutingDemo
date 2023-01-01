#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include "server.h"

int ServerBase::receiveMessage(int fd, char* buffer, size_t size, int timeout)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeout * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    int res = recv(fd, buffer, size, 0);
    if (res > 0)
    {
        return res;
    }
    else if (res == 0)
    {
        return 0; // client dropped connection
    }
    else
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            return -1;
        }
        else
        {
            return 0;
        } // unknown error, just claim client dropped it
    }
}

int Switch::init()
{
    if (mListenSocket > -1)
        return 1;

    int rc;
    int on = 1;
    struct sockaddr_in addr;

    mListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (mListenSocket < 0)
    {
        perror("socket() failed");
        return -1;
    }

    /*************************************************************/
    /* Allow socket descriptor to be reuseable                   */
    /*************************************************************/
    rc = setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof(on));
    if (rc < 0)
    {
        perror("setsockopt() failed");
        close(mListenSocket);
        return -1;
    }

    /*************************************************************/
    /* Set socket to be nonblocking. All of the sockets for      */
    /* the incoming connections will also be nonblocking since   */
    /* they will inherit that state from the listening socket.   */
    /*************************************************************/
    rc = ioctl(mListenSocket, FIONBIO, (char*) &on);
    if (rc < 0)
    {
        perror("ioctl() failed");
        close(mListenSocket);
        return -1;
    }

    /*************************************************************/
    /* Bind the socket                                           */
    /*************************************************************/
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(mPort);

    rc = bind(mListenSocket, (struct sockaddr*) &addr, sizeof(addr));
    if (rc < 0)
    {
        perror("bind() failed");
        close(mListenSocket);
        return -1;
    }

    /*************************************************************/
    /* Set the listen backlog                                    */
    /*************************************************************/
    rc = listen(mListenSocket, mMaxConns);
    if (rc < 0)
    {
        perror("listen() failed");
        close(mListenSocket);
        return -1;
    }
    return 0;
}

/**
 * non-blocking method.
 */
void Switch::run()
{
    if (!mRunning)
        return;

    mAcceptHandler = make_unique_cpp11<std::thread>([&]() { acceptHandler(); });
    mConnectionHandler = make_unique_cpp11<std::thread>([&]() { connectionHandler(); });

    printf("Server is running...\n");
}

void Switch::acceptHandler()
{
    int newSd;

    /*************************************************************/
    /* Loop waiting for incoming clients or for incoming data    */
    /* on any of the connected sockets.                          */
    /*************************************************************/
    while (mRunning)
    {
        newSd = accept(mListenSocket, nullptr, nullptr);
        if (newSd < 0)
        {
            if (errno != EWOULDBLOCK && errno != EAGAIN)
            {
                fprintf(stderr, "  Server: new connection (%lu) failed to be accepted\n", mSdQueue.size() + 1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        else
        {
            fprintf(stdout, "  Server: new connection (%lu) accepted\n", mSdQueue.size() + 1);
            std::lock_guard<std::mutex> lock(mMutex);
            mSdQueue.emplace_back(newSd);
        }
    }
}

void Switch::connectionHandler()
{
    ipc_msg_t ipcMsg;
    int rc;
    int nfds;

    /*************************************************************/
    /* Loop waiting for incoming messages from already-connected */
    /* sockets.                                                  */
    /*************************************************************/
    while (mRunning)
    {
        std::deque<int> tempSdQueue;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            tempSdQueue = mSdQueue;
            nfds = mSdQueue.size();
        }

        if (nfds == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        /**********************************************************/
        /* Call poll() and wait for it to timeout.                */
        /**********************************************************/
        struct pollfd pfds[nfds];

        int count = 0;
        for (auto it : tempSdQueue)
        {
            pfds[count].fd = it;
            pfds[count].events = POLLIN;
            count++;
        }
        rc = poll(pfds, nfds, TIMEOUT);

        /**********************************************************/
        /* Check to see if the call failed.                       */
        /**********************************************************/
        if (rc < 0)
        {
            perror("  poll() failed");
            break;
        }

        /**********************************************************/
        /* Check to see if the timeout expired.                   */
        /**********************************************************/
        if (rc == 0)
        {
            // fprintf(stderr, "  poll() timed out.\n");
            continue;
        }

        /**********************************************************/
        /* One or more descriptors are readable.  Need to         */
        /* determine which ones they are.                         */
        /**********************************************************/
        for (int i = 0; i < nfds; i++)
        {
            if (!(pfds[i].revents & POLLIN))
                continue;

            /**********************************************/
            /* There is data on this socket               */
            /**********************************************/
            int fd = pfds[i].fd;
            if (fd == mListenSocket)
                continue;

            /**********************************************/
            /* Check for new messages                     */
            /**********************************************/
            messageHandler(fd);
        } // loop through selectable descriptors
    }     // while is mRunning

    fprintf(stdout, "  Server: %lu client(s) will be shut down\n", mSdQueue.size());

    /*************************************************************/
    /* Clean up all the sockets that are open                    */
    /*************************************************************/
    while (!mSdQueue.empty())
    {
        close(mSdQueue.front());
        mSdQueue.pop_front();
    }

    mClients.clear();
    close(mListenSocket);

    mRunning.store(false);
    printf("Server shut down\n");
}

void Switch::messageHandler(int fd)
{
    char buffer[1024];
    Message message;
    int rc = 0;

    /**********************************************/
    /* Loop over until all data on this socket    */
    /* is read.                                   */
    /**********************************************/
    do
    {
        /**********************************************/
        /* Route unresolved queued messages first     */
        /**********************************************/
        std::deque<Message> tempMsgQueue(mPendingMsgQueue);
        while (!tempMsgQueue.empty())
        {
            mPendingMsgQueue.pop_front();
            forwardMessage(tempMsgQueue.front());
            tempMsgQueue.pop_front();
        }

        /**********************************************/
        /* Check for new messages                     */
        /**********************************************/
        rc = receiveMessage(fd, buffer, sizeof(buffer), 10);
        if (rc < 0)
        {
            if (errno != EWOULDBLOCK)
            {
                perror("  receiveMessage() failed");
                removeConnection(fd);
            }
            break;
        }

        if (rc == 0) // connection dropped
        {
            removeConnection(fd);
            break;
        }

        /**********************************************/
        /* Data was received                          */
        /**********************************************/
        int len = rc;
        // printf("  Server: %d bytes received\n", len);

        if (len % message.getSize() != 0)
            continue; // dump packet

        /**********************************************/
        /* Fill in the message structure              */
        /**********************************************/
        char* ptr = &buffer[0];
        for (int count = 0; count < (len / message.getSize()) && rc >= 0; count++)
        {
            memcpy(&message, ptr, message.getSize());
            ptr += message.getSize();

            // printf("  Server: message(%d) from member(%d) to member(%d)\n", message.getMti(), message.getSrcId(),
            // message.getDstId());
            mClients.emplace(message.getSrcId(), fd);

            /**********************************************/
            /* Forward the data to the destination client */
            /**********************************************/
            if (message.getDstId() > 0)
            {
                rc = forwardMessage(message);
            }
        }

        if (rc < 0)
        {
            perror("send() failed");
            removeConnection(fd);
        }
    } while (rc > 0);
}

int Switch::forwardMessage(Message& message)
{
    ipc_msg_t ipcMsg;
    int sentSize = 0;

    auto it = mClients.find(message.getDstId());
    if (it != mClients.end())
    {
        sentSize = send(it->second, message.getData(), message.getSize(), 0);

        // write message to Logger process's IPCQ
        ipcMsg.type = 123;
        memcpy(ipcMsg.text, &message, sizeof(message));
        msgsnd(mMsgQueueId, &ipcMsg, sizeof(ipcMsg), 0);
    }
    else
    {
        mPendingMsgQueue.emplace_back(message); // unresolved message
    }

    return sentSize;
}

void Switch::removeConnection(int fd)
{
    for (auto it = mSdQueue.begin(); it != mSdQueue.end(); it++)
    {
        if (*it == fd)
        {
            mSdQueue.erase(it);
            break;
        }
    }
    for (auto it : mClients)
    {
        if (it.second == fd)
        {
            mClients.erase(it.first);
            fprintf(stdout, "  Server: client (ID: %d) shut down\n", it.first);
            break;
        }
    }

    close(fd);
}

/**
 * non-blocking method.
 */
void Logger::run()
{
    if (!mRunning)
        return;

    mMsgQueueHandler = make_unique_cpp11<std::thread>([&]() { ipcQueueHandler(); });
}

void Logger::ipcQueueHandler()
{
    std::lock_guard<std::mutex> lock(mMutex);

    Message message;
    ipc_msg_t ipcMsg;

    while (mRunning)
    {
        // receive message
        msgrcv(mMsgQueueId, &ipcMsg, sizeof(ipcMsg), 0, 0);
        if (ipcMsg.type == 123)
        {
            memcpy(&message, ipcMsg.text, sizeof(message));
            message.printData(stdout); // save to file
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
