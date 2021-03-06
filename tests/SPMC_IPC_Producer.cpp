/*
    CopyRight (c) acer.king.wei@gmail.com 2019
*/

#include "../headers/SPMC.h"
using namespace std;

int main(int argc, char **argv)
{
    if (argc<5)
    {
        cout<<"Error!!"<<endl;
        cout<<"Format: bin Topic Pid MaxReaders BuffSize "<<endl;
        exit(0);
    }
    int pid = std::atoi(argv[2]);
    int n = std::atoi(argv[3]);
    int bufSize = std::atoi(argv[4]);
    cout<<"Topic = "<<argv[1]<<", Pid = "<< argv[2]<<", MaxReaders = " <<n <<", BuffSize = "<<bufSize<<endl;
    //
    SPMC<MemType_IPC> *shipc = new SPMC<MemType_IPC>(argv[1], (char)(pid&0xff), n, bufSize);
    if (argc>=6 && !strcmp(argv[5], "remove"))
    {
        shipc->remove();
        exit(0);
    }
    else if (argc>=6 && !strcmp(argv[5], "clear"))
    {
        shipc->clear();
    }

    string str;
    while (true)
    {
        cin>>str;
        while (!shipc->push(str.length(), str.c_str()))
        {
            std::this_thread::sleep_for(chrono::nanoseconds(1));
        }
        printf("sent %lu bytes\n", str.length());
    }
    delete shipc;
    std::this_thread::sleep_for(chrono::seconds(1));
    return 0;
}