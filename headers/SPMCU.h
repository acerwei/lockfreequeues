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

template<typename itemT>
class SPMCUBase
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
    static const size_t ItemSize = sizeof(itemT);

public:
    SPMCUBase(){}

    ~SPMCUBase(){}

    bool push(const itemT &item)
    {
        long long read_low = *read_high; // in case all readers are idling
        for (int i=0; i<max_readers; i++)
        {
            // Note:
            // don't compare&copy shared memory; 
            // otherwise may compare with old value but copy new value.
            long long temp = readers[i];
            if (temp>=0 && temp<read_low) read_low = temp;
        }
        if (read_low>=0 && *write_pos + ItemSize > read_low + size)
        {
            //printf("SPMCU buffer full write_pos=%lld, read_low=%lld\n", *write_pos, read_low);
            return false;
        }
        memmove(data + (*write_pos &mask), &item, ItemSize);
        (*write_pos) += ItemSize;
        return true;
    }

    // eager read
    size_t pop(int rid, itemT &item)
    {
        if (*read_high>=*write_pos)
            return 0;
        readers[rid] = fetch_add(read_high, ItemSize);
        // eagering, it has to read this item; otherwise the item will be lost.
        // TO IMPROVE.
        while (readers[rid]>=*write_pos)
            std::this_thread::sleep_for(chrono::nanoseconds(1));
        // reading
        memmove(&item, data + (readers[rid]&mask), ItemSize);
        readers[rid] = -1;
        return ItemSize;
    }

    void start(int rid)
    {
        assert(rid>=0 && rid<max_readers);
        printf("SPMCU started, write_pos=%lld, read_high=%lld\n", *write_pos, *read_high);
    }

    void clear()
    {
        *write_pos = 0;
        for (int i=0; i<max_readers; i++)
            readers[i] = -1;
        *read_high = 0;
        printf("SPMCU cleared\n");
    }
};

template <MemTypeT memType, typename itemT>
class SPMCU;

template <typename itemT>
class SPMCU<MemType_SingleProcess, itemT> : public SPMCUBase<itemT>
{
public:
    SPMCU(int _size, int _max_readers, int _tailer=DefaultTailerSize)
    {
        this->size = _size;
        this->max_readers = _max_readers;
        this->tailer = _tailer;
        assert(this->ItemSize <= this->tailer);
        this->mask = this->size - 1;
        assert(!(this->mask & this->size));
        //
        this->write_pos = new long long;
        this->readers = new long long[this->max_readers];
        this->read_high = new long long;
        this->data = new char[this->size + this->tailer];
        *this->write_pos = 0;
        memset(this->readers, 0, sizeof(long long) * this->max_readers);
        *this->read_high = 0;
    }

    ~SPMCU()
    {
        if (this->data)
        {
            delete this->write_pos;
            delete this->readers;
            delete this->read_high;
            delete this->data;
        }
    }
};


template<typename itemT>
class SPMCU<MemType_IPC, itemT> : public SPMCUBase<itemT>
{
protected:
    string name;
    int pid;
    key_t key;
    int shmid;
    char *shdata;

public:
    
    SPMCU(const char* _name, const char _pid, int _max_readers, const int _size, const int _tailer=DefaultTailerSize):
        name(_name), pid(_pid)
    {
        this->size = _size;
        this->tailer = _tailer;
        assert(this->ItemSize <= this->tailer);
        this->max_readers = _max_readers;
        this->mask = this->size - 1;
        assert(!(this->mask & this->size));
        init();
    }

    ~SPMCU()
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
        int acquire_size = this->size + this->tailer + sizeof(long long)*(this->max_readers+1);
        shmid = shmget(key, acquire_size, 0666 | IPC_CREAT);
        assert(shmid!=-1);
        shdata = (char*) shmat(shmid, nullptr, 0);
        assert(shdata);
        //
        this->write_pos = (long long*) shdata;
        this->readers = (long long*) (shdata + sizeof(long long));
        this->read_high = (long long*) (shdata + sizeof(long long)*(this->max_readers+1));
        this->data = shdata + sizeof(long long)*(this->max_readers+2);
        printf("IPC SPMCU started, write_pos=%lld, read_high=%lld\n", *this->write_pos, *this->read_high);
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
            printf("IPC SPMCU name=%s, pid=%d, shmid=%d removed\n", name.c_str(), pid, shmid);
        }
    }
};

#undef fetch_add