/* 
 * File:   TCPClient.cpp
 * Author: Dmitry Shurbin
 *
 * Created on August 12, 2020, 2:54 PM
 * This is a simple asynchronous TCP Client
 */

#include "TCPClient.h"

using namespace std;

TCPClient::TCPClient(){
    this->auxilary = NULL;
    this->sockfd = -1;
    this->stopThread = false;
    this->stopped = true;
    this->clearAllCallback();
}

int TCPClient::open(string addr, WORD port, void* arg){
    
    if (this->sockfd > 0){
        logMessage(LOGGER_ERROR, "TCPClient::open() already opened. Close it first.");
        return -4;
    }
    
    int t = 1;
    struct sockaddr_in server;
    signal(SIGPIPE, SIG_IGN);
    this->auxilary = arg;
    this->port = port;
    this->iface = addr;    

    this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    logMessage(LOGGER_DEBUG, "TCPClient::open() socket->%d",this->sockfd);
    if (this->sockfd == -1){
        logMessage(LOGGER_ERROR, "TCPClient: can't create socket");
        return -1;
    }

    if (setsockopt(this->sockfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(int)) == -1){
        logMessage(LOGGER_ERROR, "TCPClient: can't set socket options");
        return -2;
    }

    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(this->iface.c_str());
    server.sin_port = htons(this->port);
    if (connect(sockfd, (struct sockaddr*) &server, sizeof(server)) < 0){
        logMessage(LOGGER_ERROR, "TCPClient: can't connect to address %s:%d", this->iface.c_str(), this->port);
        return -3;
    }
    try{
        if (this->onOpen)
            onOpen(0,this);
    } catch(...){
    }
    stopThread = false;
    pthread_create(&Listener, NULL, listenerProcess, (void*) this);
    return 0;
}

void* TCPClient::listenerProcess(void* arg){
    int nBytes, flags;
    char buffer[MAX_PACKET_SIZE];
    char* rcvBuf;
    TCPClient* self = (TCPClient*)arg;
    fd_set readfds;
    struct timeval tv;
    int timeout;

    self->stopped = false;
    while (!self->stopThread){
        nBytes = 0;
        errno = 0;
        FD_ZERO(&readfds);
        FD_SET(self->sockfd, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 1000000; // 1 second timeout
        timeout = select(self->sockfd+1, &readfds, NULL, NULL, &tv);
        if (timeout == 0){  // nothing to read
            continue;
        } else
        if (timeout < 0){             // on error quit
            self->stopThread = true; 
            continue;
        } else            
        if (FD_ISSET(self->sockfd, &readfds)){              // if something in the buffer to read
            nBytes = recv(self->sockfd, &buffer[0], MAX_PACKET_SIZE,0);
        } else
            continue;
        
        if (nBytes > 0){
            
            packet_t* packet = new packet_t;
            packet->self = self;
            packet->data = new char[nBytes];
            if (packet->data == NULL){
                logMessage(LOGGER_DEBUG, "TCPClient: can't allocate packet data for socket: %d. Leaving.", self->sockfd);
                delete packet;
                self->stopThread = true;
            } else {
                memcpy(packet->data, buffer, nBytes);
                packet->length = nBytes;
                pthread_t* thread = new pthread_t;
                if (pthread_create(thread, NULL, handlePacket,  (void*) packet) < 0){
                    logMessage(LOGGER_DEBUG, "TCPServer: can't create handler thread for socket: %d. Leaving.", self->sockfd);
                    self->stopThread = true;
                }
                pthread_detach(*thread);
                delete thread;
            }
        } else 
        if ((nBytes == 0)  && (errno > 0)){ // ignore EAGAIN error
            // on error or disconnection - quit
            self->stopThread = true;
            logMessage(LOGGER_DEBUG,"TCPClient::listenerProcess(%d) nBytes: %d errno: %d, closing", self->sockfd, nBytes, errno);
        }
        usleep(1000);    // prevent CPU from 100% load
    }
    
    logMessage(LOGGER_DEBUG,"TCPClient::listenerProcess() closing socket->%d",self->sockfd);
    close(self->sockfd);
    self->sockfd = -1;
    self->stopped = true;
    // callback may take long, call it with thread
    pthread_t closeThread;
    if (pthread_create(&closeThread, NULL, closeCallbackThread, (void*) self) < 0){
        logMessage(LOGGER_DEBUG, "TCPClient: can't create packet handler thread for socket: %d. Leaving.", self->sockfd);
        self->stopThread = true;
    }
    pthread_detach(closeThread);
    return NULL;
}

/*
 * message handling thread
 * arg - packet data and auxiliary info
 */
void* TCPClient::handlePacket(void* arg){
    packet_t* packet = (packet_t*) arg;
    try{
        if (packet->self->DataReceived != NULL)
            packet->self->DataReceived(packet->data, packet->length, packet->self);
        } catch(...){
    }
//    free(packet->data);
    delete [] packet->data;
    packet->data = NULL;
    packet->length = 0;
    packet->self = NULL;
    delete packet;

    return NULL;
}


void* TCPClient::closeCallbackThread(void* arg){
    TCPClient* self = (TCPClient*) arg;
    try{
        if (self->onClose != NULL){
            self->onClose(self->id, self);
        }
    } catch(...){
    }
    
    return NULL;
}

void TCPClient::setReceiveCallback(std::function<void(void*, int, void*)> DataReceived){
    this->DataReceived = DataReceived;
};

void TCPClient::setOnOpenCallback(std::function<void(int, void*)> onOpen){
    this->onOpen = onOpen;
};

void TCPClient::setOnCloseCallback(std::function<void(string, void*)> onClose){
    this->onClose = onClose;
};

void TCPClient::clearAllCallback(){
    this->onClose = NULL;
    this->onOpen = NULL;
    this->DataReceived = NULL;
};

void TCPClient::closeSocket(){
    logMessage(LOGGER_DEBUG, "TCPClient::closeSocket()->%d", this->sockfd);
    this->stopThread = true;
    while (!this->stopped) // wait until thread leaves
        usleep(1000);
}

TCPClient::~TCPClient(){
    logMessage(LOGGER_DEBUG, "~TCPClient()");
    this->DataReceived = nullptr;
    this->onOpen = nullptr;
    this->onClose = nullptr;
    if (!this->stopped){
        logMessage(LOGGER_DEBUG, "~TCPClient() killing Listener");
        // probably we have to call the onClose callback

        this->stopped = true; 
        usleep(1000);
        pthread_cancel(Listener);
    }
    logMessage(LOGGER_DEBUG, "~TCPClient() cleared");
}

int TCPClient::send(void* data, int len){
    if (this->stopThread){
        logMessage(LOGGER_DEBUG, "TCPClient: can't send data (array)->%d, stopping in progress", this->sockfd);
        return -1;
    }
    fd_set writefds;
    struct timeval tv;
    int timeout;
    
    FD_ZERO(&writefds);
    FD_SET(sockfd, &writefds);
    tv.tv_sec = 0;
    tv.tv_usec = 2000000; // 2 seconds timeout
    timeout = select(sockfd+1, NULL, &writefds, NULL, &tv);
    if (timeout == 0){
        logMessage(LOGGER_DEBUG, "TCPClient: select timed out for writing socket: %d. Leaving.", sockfd);
        this->closeSocket();
        return -1;
    }
    if (FD_ISSET(sockfd, &writefds))              // if something in the buffer to read

    if (write(this->sockfd, data, len) < 0 ){
        logMessage(LOGGER_DEBUG, "TCPClient: can't send data (array)->%d, stopping", this->sockfd);
        if (!this->stopped)
            this->closeSocket();
        return -1;
    }
    return 0;
}

int TCPClient::send(string s){
    return this->send((void*)s.data(), s.length());
}