/* 
 * File:   UDPServer.h
 * Author: Dmitry Shurbin
 *
 * Created on August 12, 2020, 2:54 PM
 * This is a simple UDP server class
 */

#ifndef UDP_SERVER_H
#define UDP_SERVER_H
#include <cstdint>
#include <string>
#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#include <functional>
#include <list>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <vector>
#include <algorithm>

#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE             64*1024
#endif
#ifndef WORD
#define WORD uint32_t
#endif
#ifndef MIN_SOCKET
#define MIN_SOCKET                  51000
#endif
#ifndef MAX_SOCKET
#define MAX_SOCKET                  55001
#endif

using namespace std;

class UDPServer{
    
    struct processStruct{
        pthread_t thread;
        int client_sock;
    };
    
    struct handler_data{
        UDPServer *instance;
        int socket;
    };
    
    
    private:
        WORD port;      // port to listen to
        void* auxilary; // ayxilary pointer to a data if needed
        string iface;   // interface to listen to
        int serverSocket;
        pthread_t Listener; // Main Server Listener
        struct sockaddr_in server;
        
        static void* Listener_process(void* arg); 
        std::function<void(void*, int, uint16_t, void*)> data_handler_callback;
        WORD findFreePortPairInRange(WORD min, WORD max);
        
    public:

        UDPServer();
        uint16_t init(WORD port, string interface, void* arg);
        //virtual ~TCPServer();
        void setDataHandlerCallback(std::function<void(void*, int, uint16_t, void*)>);
        void removeDataHandlerCallback();
        void send(void* data, int len);
        void sendTo(void* data, int len, string addr, WORD port);
        void send(string s);
        void sendTo(string s, string addr, WORD port);
   
};

#endif /* UDP_SERVER_H */

