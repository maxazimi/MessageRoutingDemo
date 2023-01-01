/*
 * Created by Max Azimi on 5/18/2022 AD.
 */
#ifndef MESSAGE_H
#define MESSAGE_H

#include <memory>
#include <cstdint>
#include <cstring>
#include <cstdio>

#define BASE_PORT 49153
#define TIMEOUT 1000 /* milliseconds */
#define FILENAME "messages.msg"

// structure for message queue
typedef struct
{
    long type;
    char text[100];
} ipc_msg_t;

typedef union // 4 bytes
{
    uint32_t val;
    uint8_t buf[4];
} mti_t;

typedef union // 32 bytes of data
{
    struct
    {
        uint32_t packet_size;
        mti_t mti;
        uint8_t src_id[3];
        uint8_t dst_id[3];
        // unsigned src_id : 24;
        uint8_t trace[6];
        uint8_t pan[16];
    };
    uint8_t ptr[36];
} isc_msg_t;

class Message
{
  public:
    Message()
    {
        data.packet_size = sizeof(isc_msg_t) - sizeof(data.packet_size);
        setId(0, 0);

        for (int i = 0; i < 6; i++)
            data.trace[i] = i + 1;

        data.trace[0] = 0; // to indicate that the message is not reply yet
        memset(data.pan, 1, 16); // constant data
    }

    explicit Message(const char *buffer)
    {
        for (size_t i = 0; i < getSize(); i++)
        {
            data.ptr[i] = buffer[i];
        }
    }

    ~Message() = default;

    void setId(int src, int dst)
    {
        if (src > -1)
        {
            data.src_id[0] = src & 0xff;
            data.src_id[1] = (src >> 8) & 0xff;
            data.src_id[2] = (src >> 16) & 0xff;
        }
        if (dst > -1)
        {
            data.dst_id[0] = dst & 0xff;
            data.dst_id[1] = (dst >> 8) & 0xff;
            data.dst_id[2] = (dst >> 16) & 0xff;
        }
    }

    int getSrcId()
    {
        return data.src_id[0] + (data.src_id[1] << 8) + (data.src_id[2] << 16);
    }
    int getDstId()
    {
        return data.dst_id[0] + (data.dst_id[1] << 8) + (data.dst_id[2] << 16);
    }

    uint32_t& getMti()
    {
        return data.mti.val;
    }

    uint8_t* getData()
    {
        return data.ptr;
    }

    void printData(FILE* pfile)
    {
        if (!isReply())
            fprintf(pfile, "Request message:\tmember(%d) received %u from member(%u)\t", getDstId(), getMti(),
                    getSrcId());
        else
            fprintf(pfile, "Reply message:\tmember(%d) received %u from member(%u)\t", getDstId(), getMti(),
                    getSrcId());

        fprintf(pfile, "%03d", getSrcId());
        fprintf(pfile, "%04d", getMti());
        for (unsigned char i : data.trace)
            fprintf(pfile, "%d", i);
        for (unsigned char i : data.pan)
            fprintf(pfile, "%d", i);
        fprintf(pfile, "%03d\n", getDstId());
    }

    /**
     * It is deliberately used to check whether
     * the message received is final or is it
     * required to be processed. (MTI + 10)
     * @param value
     */
    void setReply(int value)
    {
        data.trace[0] = value;
    }
    int isReply() const
    {
        return data.trace[0];
    }

    int getSize() const
    {
        return sizeof(data);
    }

  private:
    isc_msg_t data; // 32 bytes
};

template <typename T, typename... Args> std::unique_ptr<T> make_unique_cpp11(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

#endif // MESSAGE_H
