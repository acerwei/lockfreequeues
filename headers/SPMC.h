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

#define fetch_add(x, v) __sync_fetch_and_add(x, v)

class SPMCBase
{
protected:
    int size = 0;
    int max_readers;
    int tailer = 0;
    long long *write_pos = nullptr;
    long long *readers = nullptr;
    long long *read_high = nullptr;
    char* data = nullptr;
    int mask;

public:
    static const size_t LenBytes = sizeof(uint16_t);

public:
    SPMCBase(){}

    ~SPMCBase(){}

    bool push(int len, const char* buf)
    {
        assert(len + LenBytes <= tailer);
        long long read_low = readers[0];
        for (int i=1; i<max_readers; i++)
        {
            // Note:
            // don't compare&copy shared memory; 
            // otherwise may compare with old value but copy new value.
            long long temp = readers[i];
            if (temp>=0 && temp<read_low) read_low = temp;
        }
        if (read_low>=0 && *write_pos + len + LenBytes > read_low + size)
        {
            // printf("SPMC buffer full write_pos=%lld, read_low=%lld\n", *write_pos, read_low);
            return false;
        }
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
        // potential overhead
        long long read_pos = fetch_add(read_high, 0);
        int len = 0;
        if (read_pos >= *write_pos) 
            return 0;
        memmove(&len, data + (read_pos&mask), LenBytes);
        //
        long long read_pos_now = fetch_add(read_high, len + LenBytes); // compete
        if (read_pos_now!=read_pos) //loser
            return 0;
        // reading
        readers[rid] = read_pos; 
        memmove(buf, data + ((readers[rid]+LenBytes)&mask), len);
        read_pos += (len + LenBytes);
        readers[rid] = -1; // back to idle
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
        printf("SPMC started, write_pos=%lld, read_high=%lld\n", *write_pos, *read_high);
        long long start_pos = ((*write_pos) & (~mask));
        // reader missed too much. You maynot want this.
        if (start_pos>*read_high)
        {
            readers[rid] = start_pos;
            printf("SPMC started, set read_high=%lld\n", *read_high);
        }
    }

    void keep_up(int rid)
    {
        readers[rid] = *write_pos;
        printf("SPMC keep_up, write_pos=%lld, read_pos=%lld\n", *write_pos, readers[rid]);
    }

    void clear()
    {
        *write_pos = 0;
        for (int i=0; i<max_readers; i++)
            readers[i] = -1;
        *read_high = 0;
        printf("SPMC cleared\n");
    }
};

template <MemTypeT memTypeT=MemType_SingleProcess>
class SPMC;

template <>
class SPMC<MemType_SingleProcess> : public SPMCBase
{
public:
    SPMC(int _size, int _max_readers, int _tailer)
    {
        size = _size;
        max_readers = _max_readers;
        tailer = _tailer;
        mask = size - 1;
        assert(!(mask & size));
        //
        write_pos = new long long;
        readers = new long long[max_readers];
        read_high = new long long;
        data = new char[size+tailer];
        *write_pos = 0;
        memset(readers, 0, sizeof(long long)*max_readers);
        *read_high = 0;
    }

    ~SPMC()
    {
        if (data)
        {
            delete write_pos;
            delete readers;
            delete read_high;
            delete data;
        }
    }
};

template<>
class SPMC<MemType_IPC> : public SPMCBase
{
protected:
    string name;
    int pid;
    key_t key;
    int shmid;
    char *shdata;

public:
    
    SPMC(const char* _name, const char _pid, int _max_readers, const int _size, const int _tailer=32):
        name(_name), pid(_pid)
    {
        size = _size;
        tailer = _tailer;
        max_readers = _max_readers;
        mask = size - 1;
        assert(!(mask & size));
        init();
    }

    ~SPMC()
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
        read_high = (long long*) (shdata + sizeof(long long)*(max_readers+1));
        data = shdata + sizeof(long long)*(max_readers+2);
        printf("IPC SPMC started, write_pos=%lld, read_high=%lld\n", *write_pos, *read_high);
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
            printf("IPC SPMC name=%s, pid=%d, shmid=%d removed\n", name.c_str(), pid, shmid);
        }
    }
};
