/* 
 * File:   TCPClient.h
 * Author: Dmitry Shurbin
 *
 * Created on August 12, 2020, 2:54 PM
 * This is a simple asynchronous TCP Client
 */

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <pthread.h>
#include <string>
#include <cstring>
#include <ostream>
#include <iostream>
#include <functional>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include "Logger.h"

#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE             64*1024
#endif
#ifndef WORD
#define WORD uint32_t
#endif

using namespace std;

class TCPClient{
    public:
    struct packet_t{            
            int length;
            char* data;
            TCPClient* self;

    };
        
        void* auxilary;
        int sockfd;
        string id;
        
        TCPClient();
        ~TCPClient();
        int open(string addr, WORD port, void* arg);
        int send(void* data, int len);
        int send(string s);
        void closeSocket();
        void setReceiveCallback(std::function<void(void*, int, void*)>);
        void setOnOpenCallback(std::function<void(int, void*)>);
        void setOnCloseCallback(std::function<void(string id, void*)>);
        void clearAllCallback();
        int getSocket(){return sockfd;};
        bool quitting(){return stopThread;};

    private:
        WORD port;
        string iface;
        bool stopThread;
        bool stopped;
        pthread_t Listener;
        
        static void* listenerProcess(void* arg);
        static void* closeCallbackThread(void* arg);
        static void* handlePacket(void* arg);
        std::function<void(void*, int, void*)>DataReceived;
        std::function<void(int, void*)>onOpen;
        std::function<void(string id, void*)>onClose;
};

#endif /* TCP_CLIENT_H */

