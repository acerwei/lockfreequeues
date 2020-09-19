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

class SPBCBase
{
protected:
    int size = 0;
    int max_readers;
    int tailer = 0;
    long long *write_pos = nullptr;
    long long *readers = nullptr;
    char* data = nullptr;
    int mask;

public:
    static const size_t LenBytes = sizeof(uint16_t);

public:
    SPBCBase(){}

    ~SPBCBase(){}

    bool push(int len, const char* buf)
    {
        assert(len + LenBytes <= tailer);
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

    size_t pop(int rid, char* buf)
    {
        assert(buf);
        if (readers[rid] >= *write_pos)
            return 0;
        int len = 0;
        memmove(&len, data + (readers[rid]&mask), LenBytes);
        memmove(buf, data + ((readers[rid]+LenBytes)&mask), len);
        readers[rid] += (len + LenBytes);
        return len;
    }
    
    // use this if you know exactly what the item is about
    template <typename itemT>
    size_t pop(int rid, itemT &item)
    {
        int len = pop(rid, (char*)(&item));
        assert(!len || len == sizeof(itemT));
        return len;
    }

    void start(int rid)
    {
        assert(rid>=0 && rid<max_readers);
        printf("SPBC started, write_pos=%lld, read_pos=%lld\n", *write_pos, readers[rid]);
        long long start_pos = ((*write_pos) & (~mask));
        // reader missed too much.
        if (start_pos>readers[rid])
        {
            readers[rid] = start_pos;
            printf("SPBC started, set read_pos=%lld\n", readers[rid]);
        }
    
    }

    void keep_up(int rid)
    {
        readers[rid] = *write_pos;
        printf("SPBC keep_up, write_pos=%lld, read_pos=%lld\n", *write_pos, readers[rid]);
    }

    void clear()
    {
        *write_pos = 0;
        for (int i=0; i<max_readers; i++)
            readers[i] = 0;
        printf("SPBC cleared\n");
    }
};

template <MemTypeT memTypeT=MemType_SingleProcess>
class SPBC;

template <>
class SPBC<MemType_SingleProcess> : public SPBCBase
{
public:
    SPBC(int _size, int _max_readers, int _tailer)
    {
        size = _size;
        max_readers = _max_readers;
        tailer = _tailer;
        mask = size - 1;
        assert(!(mask & size));
        //
        write_pos = new long long;
        readers = new long long[max_readers];
        data = new char[size+tailer];
        *write_pos = 0;
        memset(readers, 0, sizeof(long long)*max_readers);
    }

    ~SPBC()
    {
        if (data)
        {
            delete write_pos;
            delete readers;
            delete data;
        }
    }
};

template<>
class SPBC<MemType_IPC> : public SPBCBase
{
protected:
    string name;
    int pid;
    key_t key;
    int shmid;
    char *shdata;

public:
    
    SPBC(const char* _name, const char _pid, int _max_readers, const int _size, const int _tailer=32):
        name(_name), pid(_pid)
    {
        size = _size;
        tailer = _tailer;
        max_readers = _max_readers;
        mask = size - 1;
        assert(!(mask & size));
        init();
    }

    ~SPBC()
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
        shmid = shmget(key, size+tailer+sizeof(int)*(max_readers+1), 0666 | IPC_CREAT);
        assert(shmid!=-1);
        shdata = (char*) shmat(shmid, nullptr, 0);
        assert(shdata);
        //
        write_pos = (long long*) shdata;
        readers = (long long*) (shdata + sizeof(long long));
        data = shdata + sizeof(long long) + sizeof(long long)*max_readers;
        printf("IPC SPBC started, write_pos=%lld\n", *write_pos);
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
            printf("IPC SPBC name=%s, pid=%d, shmid=%d removed\n", name.c_str(), pid, shmid);
        }
    }
};