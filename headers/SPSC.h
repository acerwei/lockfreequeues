/*
    CopyRight (c) acer.king.wei@gmail.com 2019
*/

#pragma once

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <cassert>
#include <sys/shm.h>
#include <sys/ipc.h>
#include "MemType.h"
using namespace std;

class SPSCBase
{
protected:
    int size = 0;
    int tailer = 0;
    long long *write_pos = nullptr, *read_pos = nullptr;
    char* data = nullptr;
    int mask;

public:
    static const size_t LenBytes = sizeof(uint16_t);

public:
    SPSCBase(){}

    ~SPSCBase(){}

    bool push(int len, const char* buf)
    {
        assert(len + LenBytes <= tailer);
        if (*write_pos+len+LenBytes > *read_pos+size)
            return false;
        memmove(data + (*write_pos&mask), &len, LenBytes);
        memmove(data + ((*write_pos+LenBytes)&mask), buf, len);
        (*write_pos) += (len+LenBytes);
        return true;
    }

    template <typename itemT>
    bool push(const itemT &item)
    {
        return push(sizeof(itemT), (const char*)(&item));
    }

    size_t pop(char* buf)
    {
        assert(buf);
        if (*read_pos >= *write_pos)
            return 0;
        int len = 0;
        memmove(&len, data + (*read_pos&mask), LenBytes);
        memmove(buf, data + ((*read_pos+LenBytes)&mask), len);
        *read_pos += (len + LenBytes);
        return len;
    }
    
    // use this if you know exactly what the item is about
    template <typename itemT>
    size_t pop(itemT &item)
    {
        int len = pop((char*)(&item));
        assert(!len || len == sizeof(itemT));
        return len;
    }

    void keep_up()
    {
        *read_pos = *write_pos;
        printf("SPSC keep_up, write_pos=%lld, read_pos=%lld\n", *write_pos, *read_pos);
    }

    void clear()
    {
        *read_pos = 0;
        *write_pos = 0;
        printf("SPSC cleared, write_pos=%lld, read_pos=%lld\n", *write_pos, *read_pos);
    }
};

template <MemTypeT memTypeT=MemType_SingleProcess>
class SPSC;

template <>
class SPSC<MemType_SingleProcess> : public SPSCBase
{
public:
    SPSC(int _size, int _tailer=DefaultTailerSize)
    {
        size = _size;
        tailer = _tailer;
        mask = size - 1;
        assert(!(mask & size));
        //
        write_pos = new long long;
        read_pos = new long long;
        data = new char[size+tailer];
        *write_pos = 0;
        *read_pos = 0;
    }

    ~SPSC()
    {
        if (data)
        {
            delete write_pos;
            delete read_pos;
            delete data;
        }
    }
};

template<>
class SPSC<MemType_IPC> : public SPSCBase
{
protected:
    string name;
    int pid;
    key_t key;
    int shmid;
    char *shdata;

public:
    
    SPSC(const char* _name, const char _pid, const int _size, const int _tailer=DefaultTailerSize):
        name(_name), pid(_pid)
    {
        size = _size;
        tailer = _tailer;
        mask = size - 1;
        assert(!(mask & size));
        init();
    }

    ~SPSC()
    {
        if (shdata)
        {
            shmdt(shdata);
            cout<<"shmid detached"<<endl;
        }
    }

    void init()
    {
        key = ftok(name.c_str(), pid);
        assert(key!=-1);
        shmid = shmget(key, size+tailer+sizeof(long long)*2, 0666 | IPC_CREAT);
        assert(shmid!=-1);
        shdata = (char*) shmat(shmid, nullptr, 0);
        assert(shdata);
        //
        write_pos = (long long*) shdata;
        read_pos = (long long*) (shdata + sizeof(long long));
        data = shdata + sizeof(long long)*2;
        printf("IPC SPSC started, write_pos=%lld, read_pos=%lld\n", *write_pos, *read_pos);
        // reader missed too much.
        long long start_pos = ((*write_pos) & (~mask));
        if (start_pos>*read_pos)
        {
            *read_pos = start_pos;
            printf("SPMC started, set read_pos=%lld\n", *read_pos);
        }
    }

    void remove()
    {
        if (shmid)
        {
            if (shdata)
            {
                shmdt(shdata);
                cout<<"shmid detached"<<endl;
            }
            shdata = nullptr;
            shmctl(shmid, IPC_RMID, nullptr);
            printf("IPC SPSC name=%s, pid=%d, shmid=%d removed\n", name.c_str(), pid, shmid);
        }
    }
};
