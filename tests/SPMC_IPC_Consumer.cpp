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
    cout<<"Topic = "<<argv[1]<<", Pid = "<< argv[2]<<",MaxReaders = " <<n <<", BuffSize = "<<bufSize<<endl;
    cout<<"My Reader ID is "<<rid<<endl;
    //
    SPMC<MemType_IPC, double> *shipc = new SPMC<MemType_IPC, double>(argv[1], (char)(pid&0xff), n, bufSize);
    shipc->start(rid);
    int received = 0;
    double val = 0;
    while (true)
    {
        size_t len = shipc->pop(rid, val);
        if (len)
        {
            received++;
            cout<<val<<" "<<received<<endl;
        }
        else
        {
            std::this_thread::sleep_for(chrono::nanoseconds(1));
        }
        
    }
    
    delete shipc;
    std::this_thread::sleep_for(chrono::seconds(1));
    return 0;
}