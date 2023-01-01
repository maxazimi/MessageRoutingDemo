#include <stdexcept>
#include <cstring>
#include "client.h"

Member::Member(std::string remoteAddress, int remotePort, int srcId) : mRunning(true)
{
    mId = srcId;
    memset(&mRemoteAddr, 0, sizeof(mRemoteAddr));
    mRemoteAddr.sin_family = AF_INET;
    mRemoteAddr.sin_addr.s_addr = inet_addr(remoteAddress.c_str());
    mRemoteAddr.sin_port = htons(remotePort);

    mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (mSocket < 0)
        throw std::runtime_error("socket() failed");

    if (connectToServer() != 0)
        throw std::runtime_error("connectToServer() failed");

    mThread = make_unique_cpp11<std::thread>([&]() { run(); });
}

Member::~Member()
{
    mRunning = false;
    mThread->join();
    close(mSocket);
    printf("Member::~Member() called\n");
}

int Member::connectToServer()
{
    if (connect(mSocket, (struct sockaddr*) &mRemoteAddr, sizeof(struct sockaddr_in)) != 0)
        return -1;

    mMessage.setId(mId, 0);
    return send(mSocket, mMessage.getData(), mMessage.getSize(), 0) == mMessage.getSize() ? 0 : -2;
}

int Member::sendMessage(int mti, int dst_id, bool is_reply)
{
    mMessage.setId(mId, dst_id);
    if (mti >= 0)
        mMessage.getMti() = mti;
    mMessage.setReply(is_reply);

    return send(mSocket, mMessage.getData(), mMessage.getSize(), 0);
}

int Member::receiveMessage(Message& message, int timeout)
{
    if (timeout >= 0)
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = timeout * 1000; // milliseconds
        setsockopt(mSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    int res = recv(mSocket, message.getData(), message.getSize(), 0);
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

void Member::run()
{
    std::lock_guard<std::mutex> lock(mMutex);

    Message message;
    int result;

    while (mRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        result = receiveMessage(message, -1);
        if (result < message.getSize())
        {
            if (result == 0) // connection dropped
            {
                mRunning.store(false);
                break;
            }
            continue;
        }

        if (message.getSrcId() == 0 || message.getDstId() != mId) // drop message
            continue;

        if (!message.isReply()) // is not reply
        {
            printf("Request message: %u from member(%u)\n", message.getMti(), message.getSrcId());
            sendMessage(message.getMti() + 10, message.getSrcId(), true);
        }
        else
        {
            printf("Reply message: %u from member(%u)\n", message.getMti(), message.getSrcId());
        }
    }
}
