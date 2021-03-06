/*
    CopyRight (c) acer.king.wei@gmail.com 2019
*/

#include "../headers/SPMC.h"
using namespace std;

int main(int argc, char **argv)
{
    if (argc<6)
    {
        cout<<"Error!!"<<endl;
        cout<<"Format: bin Topic Pid MaxReaders BuffSize "<<endl;
        exit(0);
    }
    int pid = std::atoi(argv[2]);
    int n = std::atoi(argv[3]);
    int bufSize = std::atoi(argv[4]);
    int rid = std::atoi(argv[5]);
    cout<<"Topic = "<<argv[1]<<", Pid = "<< argv[2]<<", MaxReaders = " <<n <<", BuffSize = "<<bufSize<<endl;
    cout<<"My Reader ID is "<<rid<<endl;
    //
    SPMC<MemType_IPC> *shipc = new SPMC<MemType_IPC>(argv[1], (char)(pid&0xff), n, bufSize);
    shipc->start(rid);
    char buf[64];
    int received = 0;
    while (true)
    {
        size_t len = shipc->pop(rid, buf);
        if (!len)
            std::this_thread::sleep_for(chrono::nanoseconds(1));
        else
        {
            received++;
            buf[len] = '\0';
            cout<<buf<<" "<<len<<" "<<received<<endl;
        }
    }
    
    delete shipc;
    std::this_thread::sleep_for(chrono::seconds(1));
    return 0;
}