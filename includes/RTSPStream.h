/* 
 * File:   RTSPStream.h
 * Author: Dmitry Shurbin
 *
 * Created on May 12, 2021, 5:39 PM
 * This is the class which is connecting to an RTSP source and delivers 
 * raw RTP frames
 */

#ifndef RTSPSTREAM_H
#define RTSPSTREAM_H
#include <string>
#include <functional>
#include <netdb.h>	//hostent
#include "TCPClient.h"
#include "UDPServer.h"
#include "md5.h"
#include "base64.h"

#define USER_AGENT              "User-Agent: RTSP-Proxy\r\n"
#define STATE_INIT             -1
#define STATE_OPTIONS           0
#define STATE_DESCRIBE          1
#define STATE_SETUP             2
#define STATE_PLAY              3


using namespace std;

struct SReport_t{
    uint8_t    VPRC;
    uint8_t    PT;
    uint8_t    lenH;
    uint8_t    lenL;
    uint32_t   SSRC;
    uint32_t   NTP_H;
    uint32_t   NTP_L;
    uint32_t   RTP_TS;
    uint32_t   pCount;
    uint32_t   oCount;
};

struct RReport_t{
    uint8_t    VPRC;
    uint8_t    PT;
    uint8_t    lenH;
    uint8_t    lenL;
    uint32_t   mySSRC;
    uint32_t   SSRC;
    uint32_t   lost;
    uint32_t   pCount;
    uint32_t   jitter;
    uint32_t   ntp32;    // LSR
    uint32_t   DSR;
};

union RReport_u{
    uint8_t    packet[256];
    struct RReport_t report;
};

union SReport_u{
    uint8_t    packet[256];
    struct SReport_t report;
};
    
class RTSPStream{
    public:
        RTSPStream();
        ~RTSPStream();
        int open(string uri,void* arg);
        void close();
        void setOnPacketCallback(std::function<void(void*, int, void*)>);
        void setOnCloseCallback(std::function<void(void*)>);
        void setOnOpenCallback(std::function<void(void*)>);

    protected:
        void* aux;
        TCPClient client;
        UDPServer udpCh1;
        UDPServer udpCh2;
        WORD localPortCh1, localPortCh2;
        WORD portCh1, portCh2;
        uint8_t primer[4];
        string uri;
        string uriMod;
        string proto;
        string hostname;
        string ip;
        uint16_t port;
        string request;
        string username;
        string password;
        string realm;
        string nonce;
        string authType;
        string sdp;
        int CSeq;
        bool authenticated;
        int state;
        
        int parseURI();
        int hostname_to_ip(char * hostname , char* ip);
        string makeAuthString(string cmd);
        string buildCommand(string cmd, string payload);
        int checkAuth(string req);
        void onPacket(void* data, int dataLen);
        void onClose();
        static void onTCPOpen(int socket, void* arg);
        static void onTCPData(void* data, int dataLen, void* arg);
        static void onTCPClose(string id, void* arg);
        
        static void udpCh1Receiver(void* data, int nBytes, uint16_t port, void* aux);
        static void udpCh2Receiver(void* data, int nBytes, uint16_t port, void* aux);
        void streamReady();

        std::function<void(void*, int, void*)> on_packet_callback;
        std::function<void(void*)> on_close_callback;
        std::function<void(void*)> on_open_callback;
};

#endif /* RTSPSTREAM_H */

