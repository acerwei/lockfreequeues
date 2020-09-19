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
    if (argc>=5 && !strcmp(argv[4], "remove"))
    {
        shipc->remove();
        exit(0);
    }
    else if (argc>=5 && !strcmp(argv[4], "clear"))
    {
        shipc->clear();
    }
    string str;
    while (true)
    {
        cin>>str;
        int val = std::atoi(str.c_str());
        if (!val)
        {
            while (!shipc->push(str.length(), str.c_str()))
            {
                std::this_thread::sleep_for(chrono::nanoseconds(1));
            }
            printf("sent %lu bytes\n", str.length());
        }
        else
        {
            while (!shipc->push(val))
            {
                std::this_thread::sleep_for(chrono::nanoseconds(1));
            }
            printf("sent int\n");
        }
        
    }
    delete shipc;
    std::this_thread::sleep_for(chrono::seconds(1));
    return 0;
}