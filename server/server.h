/*
 * Created by Max Azimi on 5/18/2022 AD.
 */
#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <deque>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <fcntl.h>
#include "isc_msg.h"

#define LOG_ERROR(err)                                                                                                 \
    fprintf(stderr, "ERROR:\tfrom %s,\tline (%d),\tfunction %s failed --> %s\n", __FILE__, __LINE__, __FUNCTION__, err)
#define LOG_DEBUG(log)                                                                                                 \
    fprintf(stdout, "DEBUG:\tfrom %s,\tline (%d),\tfunction %s --> %s\n", __FILE__, __LINE__, __FUNCTION__, log)

class ServerBase
{
  public:
    ServerBase(int port = BASE_PORT, int maxClients = 999)
        : mPort(port), mMaxConns(maxClients), mListenSocket(-1), mRunning(true)
    {
        key_t msgKey = ftok("iscChallenge.isc", 'B');
        mMsgQueueId = msgget(msgKey, 0666 | IPC_CREAT);
    }

    virtual ~ServerBase()
    {
    }

    virtual void run() = 0;
    void deinit()
    {
        mRunning = false;
    }
    int getPort() const
    {
        return mPort;
    }
    bool isRunning() const
    {
        return mRunning.load();
    }

  protected:
    virtual int init() = 0;
    int receiveMessage(int fd, char* buffer, size_t size, int timeout);

    int mPort;
    int mListenSocket;
    int mMaxConns;
    int mMsgQueueId;

    std::mutex mMutex{};
    std::atomic_bool mRunning; // used to be able to terminate background threads
};

class Switch : public ServerBase
{
  public:
    Switch(int port = BASE_PORT, int maxClients = 999) : ServerBase(port, maxClients)
    {
        if (init() != 0)
            throw std::runtime_error("Switch::init() failed");
    }
    ~Switch() final
    {
        mRunning = false;

        if (mAcceptHandler != nullptr)
            mAcceptHandler->join();

        if (mConnectionHandler != nullptr)
            mConnectionHandler->join();

        if (mMsgQueueHandler != nullptr)
            mMsgQueueHandler->join();
    }

    void run() override;

  private:
    int init() override;
    int forwardMessage(Message& message);

    void acceptHandler();
    void connectionHandler();
    void messageHandler(int fd);
    void removeConnection(int fd);

    std::deque<int> mSdQueue{};
    std::deque<Message> mPendingMsgQueue{};

    std::unordered_map<int, int> mClients{}; // id --> socket
    pid_t mChildId;

    std::unique_ptr<std::thread> mAcceptHandler;
    std::unique_ptr<std::thread> mConnectionHandler;
    std::unique_ptr<std::thread> mMsgQueueHandler;
};

class Logger : public ServerBase
{
  public:
    Logger()
    {
        mFilePtr = fopen(FILENAME, "w");
    }
    ~Logger() override
    {
        mRunning = false;
        msgctl(mMsgQueueId, IPC_RMID, nullptr); // destroy the message queue

        printf("~Logger() called\n");
        
        if (mMsgQueueHandler != nullptr)
            mMsgQueueHandler->join();

        fclose(mFilePtr);
    }

    int init() override
    {
        return 0;
    }
    void run() override;

  private:
    void ipcQueueHandler();

    FILE* mFilePtr = nullptr; // used to save messages to a file
    std::unique_ptr<std::thread> mMsgQueueHandler;
};

#endif // SERVER_H
