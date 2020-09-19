/*
    CopyRight (c) acer.king.wei@gmail.com 2019
*/

#include "../headers/SPSC.h"
using namespace std;

int main(int argc, char **argv)
{
    if (argc<4)
    {
        cout<<"Error!!"<<endl;
        cout<<"Format: bin Topic Pid BuffSize"<<endl;
        exit(0);
    }
    int pid = std::atoi(argv[2]);
    int bufSize = std::atoi(argv[3]);
    cout<<"Topic = "<<argv[1]<<", Pid = "<< argv[2]<<", BuffSize = "<<bufSize<<endl;
    //
    SPSC<MemType_IPC> *shipc = new SPSC<MemType_IPC>(argv[1], (char)(pid&0xff), bufSize);
    if (argc>=5 && !strcmp(argv[4], "int"))
    {
        int val;
        while (true)
        {
            size_t len = shipc->pop(val);
            if (!len)
                std::this_thread::sleep_for(chrono::nanoseconds(1));
            else
            {
                cout<<val<<endl;
            }
        }
    }
    else
    {
        if (argc>=5 && !strcmp(argv[4], "keepup"))
        {
            shipc->keep_up();
        }
        char buf[64];
        while (true)
        {
            size_t len = shipc->pop(buf);
            if (!len)
                std::this_thread::sleep_for(chrono::nanoseconds(1));
            else
            {
                buf[len] = '\0';
                cout<<buf<<" "<<len<<endl;
            }
        }
    }
    
    delete shipc;
    std::this_thread::sleep_for(chrono::seconds(1));
    return 0;
}