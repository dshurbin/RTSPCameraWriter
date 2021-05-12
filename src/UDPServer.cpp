/* 
 * File:   UDPServer.cpp
 * Author: Dmitry Shurbin
 *
 * Created on August 12, 2020, 2:54 PM
 * This is a simple UDP server class
 */

#include "UDPServer.h"

/*
 Constructor: Creating listening UDP socket
 */
UDPServer::UDPServer(){
}

/*
 * this finder is for finding free UDP port pair even and odd
 * for working with RTP
 * so, "min" parameter should be even
 */
WORD UDPServer::findFreePortPairInRange(WORD min, WORD max){
    FILE* f;
    vector<WORD> busy;
    char line[256];
    string s;
    int pos;
    
    f = fopen("/proc/net/udp", "r");
    fgets(line, sizeof(line), f); // skip first line as header
    
    while (fgets(line, sizeof(line), f)){
        s.assign(line, strlen(line));
        pos = s.find_first_not_of(" ");
        if (pos != string::npos)
            s = s.substr(pos); // trim left spaces
        
        if ((pos = s.find(":")) != string::npos){
            s = s.substr(pos+1);
            pos = s.find(":");
            s = s.substr(pos+1);// copy the port number
            pos = s.find(" ");
            s = "0x" + s.substr(0, pos);// copy the port number
            WORD p;
            try{
                p = stoi(s, nullptr, 0);
                
                if (!std::count(busy.begin(), busy.end(), p)) // find if port already present
                    busy.push_back(p);
            } catch (...){
                
            }
        }
    }
    fclose(f);
    for (WORD i = min; i < max; i+=2){
        if ((!std::count(busy.begin(), busy.end(), i)) && (!std::count(busy.begin(), busy.end(), i+1))){
            //cout << "found UDP pair " << to_string(i) << "-" << to_string(i+1) << endl;
            return i;
        } 
    }
    
    return 0;
}

uint16_t UDPServer::init(WORD port, string interface, void* arg){
    
    int t = 1;
    this->port = port;
    this->iface = interface;

    //Creating UDP socket
    this->serverSocket = socket(AF_INET , SOCK_DGRAM , 0);
    if (this->serverSocket == -1){
        std::cout << "UDPServer: Error. Could not create socket." << std::endl;
        return 0;
    }

    //filling in server data
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    
    if (this->port == 0){
        this->port = MIN_SOCKET;
        int bindRes;
        this->port =findFreePortPairInRange(MIN_SOCKET, MAX_SOCKET);
        if (this->port == 0) {
            std::cout << "UDPServer: Error. finding free UDP ports." << std::endl;
            return 0;
        }
    } 
    
    server.sin_port = htons( this->port );
     
    //Binding
    if( bind(this->serverSocket,(struct sockaddr *)&server , sizeof(server)) < 0){
        std::cout << "UDPServer: Error. Binding failed." << std::endl;
        return 0;
    }
    std::cout << "UDPServer: Socket bound." << std::endl;
    
    //firing up the main server listener
    std::cout << "UDPServer: Starting Listener..." << std::endl;
    pthread_create( &Listener , NULL ,  Listener_process ,  (void*)this);    
    auxilary = arg;
 
    return this->port;    
}

/*
 Main listener process: Listening to server socket
 *  
 */
void* UDPServer::Listener_process(void* arg){
    int nBytes;
    char buffer[MAX_PACKET_SIZE];
    struct sockaddr_in cliaddr;
    socklen_t socklen;
    UDPServer* self = (UDPServer*) arg;
    
    memset(&cliaddr, 0, sizeof(cliaddr));
    socklen = sizeof(cliaddr);
    while ((nBytes = recvfrom(self->serverSocket, buffer, MAX_PACKET_SIZE, MSG_WAITALL, (struct sockaddr*) &cliaddr, &socklen)) > 0){
        char* buf = (char*)malloc(nBytes);
        memcpy(buf, buffer, nBytes); 
        self->data_handler_callback(buf, nBytes, cliaddr.sin_port, self->auxilary);
        free(buf);
    }
    return NULL;
}

void UDPServer::send(void* data, int len){
    if (sendto(this->serverSocket, data, len, 0, (struct sockaddr*)&this->server, sizeof(this->server)) < 0 ){
        cout << "UDPServer: can't send data" << endl;
    }
}

void UDPServer::sendTo(void* data, int len, string addr, WORD port){
    struct sockaddr_in si_other;
    memset((char*)&si_other, 0, sizeof(si_other)); // fill struct with 0
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(port);
    si_other.sin_addr.s_addr = inet_addr(addr.c_str());
    if (sendto(this->serverSocket, data, len, 0, (struct sockaddr*)&si_other, sizeof(si_other)) < 0 ){
        cout << "UDPServer: can't send data" << endl;
    }
}

void UDPServer::sendTo(string s, string addr, WORD port){
    struct sockaddr_in si_other;
    memset((char*)&si_other, 0, sizeof(si_other)); // fill struct with 0
    si_other.sin_family = AF_INET;
    si_other.sin_port = port;
    si_other.sin_addr.s_addr = inet_addr(addr.c_str());
    
    if (sendto(this->serverSocket, s.c_str(), strlen(s.c_str()), 0, (struct sockaddr*)&si_other, sizeof(si_other)) < 0 ){
        cout << "UDPServer: can't send data" << endl;
    }
}


void UDPServer::setDataHandlerCallback(std::function<void(void*, int, uint16_t, void*)> data_handler_callback){
 this->data_handler_callback = data_handler_callback;
};

void UDPServer::removeDataHandlerCallback(){
    this->data_handler_callback = NULL;
}