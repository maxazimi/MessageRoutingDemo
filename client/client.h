/*
 * Created by Max Azimi on 5/18/2022 AD.
 */
#ifndef CLIENT_H
#define CLIENT_H

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <deque>
#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <isc_msg.h>

class Member
{
  public:
    Member(std::string ipAddress = "127.0.0.1", int port = BASE_PORT, int srcId = 0);
    ~Member();

  private:
    int connectToServer();
    int receiveMessage(Message& message, int timeout = 5);
    void run();

  public:
    int getId() const
    {
        return mId;
    }
    int isRunning() const
    {
      return mRunning.load();
    }

    int sendMessage(int mti, int dst_id, bool is_final = false);

  private:
    int mId;
    Message mMessage;

    std::unique_ptr<std::thread> mThread;
    std::mutex mMutex;
    std::atomic_bool mRunning; // used to be able to terminate background threads

    int mSocket;
    struct sockaddr_in mRemoteAddr = {0};
};

#endif // CLIENT_H
