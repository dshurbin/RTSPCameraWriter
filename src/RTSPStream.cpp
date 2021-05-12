/* 
 * File:   RTSPStream.cpp
 * Author: Dmitry Shurbin
 *
 * Created on May 12, 2021, 5:39 PM
 * This is the class which is connecting to an RTSP source and delivers 
 * raw RTP frames
 */

#include "RTSPStream.h"

RTSPStream::RTSPStream(){
    logMessage(LOGGER_DEBUG, "RTSPStream::RTSPStream()");
    this->primer[0] = 0xCE; this->primer[1] = 0xFA; this->primer[2] = 0xED; this->primer[3] = 0xFE; // this should be sent to camera to initiate transfer
    this->localPortCh1 = 0;
    this->localPortCh2 = 0;
    this->authType = "";
    this->sdp = "";
    this->on_close_callback = NULL;
    this->on_open_callback = NULL;
    this->on_packet_callback = NULL;
    this->proto = "";
    this->hostname = "";
    this->request = "";
    this->realm = "";
    this->uri = "";
    this->uriMod = "";
    this->ip = "";
    this->port = 0;
    this->username = "";
    this->password = "";
    this->nonce = "";
    this->authType = "";
    this->sdp = "";
    this->CSeq = 0;
    this->authenticated = false;
    this->state = STATE_INIT;
}

RTSPStream::~RTSPStream(){
    logMessage(LOGGER_DEBUG, "RTSPStream::~RTSPStream()");
    this->on_close_callback = NULL;
    this->on_open_callback = NULL;
    this->on_packet_callback = NULL;
    this->client.clearAllCallback();
    this->client.closeSocket();
}

void RTSPStream::udpCh1Receiver(void* data, int nBytes, uint16_t port, void* aux){ // video frames
    RTSPStream* self = (RTSPStream*) aux;

    // handover data AS IS
    if (self->on_packet_callback)
        self->on_packet_callback(data, nBytes, self->aux);
}

/*
 * Receiver of SCTP channel
 */
void RTSPStream::udpCh2Receiver(void* data, int nBytes, uint16_t port, void* aux){ // SCTP
    RTSPStream* self = (RTSPStream*) aux;
    union SReport_u SR;
    union RReport_u RR;
    uint8_t* data8Array = (uint8_t*) data;
    
    if (nBytes < 28) // wrong report
        return;
    
    for (int i=0; i<28; i++) SR.packet[i] = data8Array[i]; 

    for (int i=0; i<sizeof(RR); i++) RR.packet[i] = 0; 
    
    // generate Receiver Report on each Sender Report
    RR.report.VPRC = 0x81;
    RR.report.PT = 201;    // Receiver Report
    RR.report.lenH = 0;
    RR.report.lenL = 7; // length is 7 DWORDs
    RR.report.mySSRC = htonl(0xFE0E0001);
    RR.report.SSRC = SR.report.SSRC;
    RR.report.lost = 0; // assume we do not lost any frames
    RR.report.pCount = SR.report.pCount; // just copy from senders report
    RR.report.jitter = htonl(1);
    RR.report.ntp32 = htonl((ntohl(SR.report.NTP_H) & 0x0000FFFF) + ((ntohl(SR.report.NTP_L) & 0xFFFF0000) >> 16));
    RR.report.DSR = htonl(1); // Delay since last report
    self->udpCh2.sendTo(RR.packet, 256, self->ip, self->portCh2);
}


int RTSPStream::open(string uri, void* arg){
    this->aux = arg;
    this->uri = uri;
    logMessage(LOGGER_DEBUG, "RTSPStream::open");
    if (this->parseURI() < 0)
        return -1;
    this->CSeq = 0;
    this->client.setOnOpenCallback(this->onTCPOpen);
    this->client.setReceiveCallback(this->onTCPData);
    this->client.setOnCloseCallback(this->onTCPClose);
    this->client.open(this->ip, this->port, this);
    return 0;
}

string RTSPStream::makeAuthString(string cmd){
    string ret;
    
    string ha1 = md5(this->username+":"+this->realm+":"+this->password);
    string ha2 = md5(cmd+":"+this->uri+this->uriMod);
    ret = "Authorization: Digest ";
    ret = ret + "username=\"" + this->username + "\", ";
    ret = ret + "algorithm=\"MD5\", ";
    ret = ret + "realm=\"" + this->realm + "\", ";
    ret = ret + "uri=\"" + this->uri + "\", ";
    ret = ret + "nonce=\"" + this->nonce + "\", ";
    ret = ret + "response=\"" + md5(ha1+":"+this->nonce+":"+ha2)+"\"\r\n";
    return ret;
}

string RTSPStream::buildCommand(string cmd, string payload){
    string ret;
    this->CSeq++;
    ret = cmd + " " + this->uri + this->uriMod + " RTSP/1.0\r\nCSeq: "+to_string(this->CSeq)+"\r\n";    
    if (payload != "")
        ret += payload+"\r\n";
    if (this->authType == "digest")
        ret += this->makeAuthString(cmd);
    else
    if (this->authType != "") // assume Basic auth
        ret += this->authType;
    
    ret += USER_AGENT;
    ret += "\r\n";
    
    return ret;
}

int RTSPStream::hostname_to_ip(char * hostname , char* ip){
	struct hostent *he;
	struct in_addr **addr_list;
	int i;
		
	if ( (he = gethostbyname( hostname ) ) == NULL) 
	{
		// get the host info
		herror("gethostbyname");
		return 1;
	}

	addr_list = (struct in_addr **) he->h_addr_list;
	
	for(i = 0; addr_list[i] != NULL; i++) 
	{
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr_list[i]) );
		return 0;
	}
	
	return 1;
}

int RTSPStream::parseURI(){
    int pos;
    this->password = "";
    this->username = "";
    this->port = 554;
    this->uriMod = "";
    
    if ((pos = this->uri.find("//")) == string::npos)
        return -1;
    this->proto = uri.substr(0,pos+2); // save proto prefix
    
    this->uri = this->uri.substr(pos+2); // remove rtsp://, http:// or whatever
    
    if ((pos = this->uri.find("@")) != string::npos){
        string t = this->uri.substr(0,pos);
        this->uri = this->uri.substr(pos+1);
        if ((pos = t.find(":")) != string::npos){
            this->username = t.substr(0,pos);
            this->password = t.substr(pos+1);
        } else
            this->username = t;
        pos = this->uri.find("@");
        this->uri = this->uri.substr(pos + 1);
    }
    if ((pos = this->uri.find(":")) != string::npos){
        int t = this->uri.find("/");
        if (t == string::npos) 
            return -1; // wrong uri
        string p = this->uri.substr(pos+1, t-pos-1);
        try{
            this->port = stoi(p, nullptr, 0);
            if (this->port <= 0)
                this->port = 554;
        } catch (...){
            this->port = 554;
        }
        this->uri.erase(pos, t - pos); // erase :port
    } else
        this->port = 554;
    
    int t = this->uri.find("/");
    if (t == string::npos)
        return -2;
    
    this->hostname = this->uri.substr(0, t);
    this->request = uri.substr(t);
    char ipc[16];
    hostname_to_ip((char*)this->hostname.c_str(), ipc);
    this->ip = string(ipc);
    this->uri = this->proto + this->hostname + ":" + to_string(this->port) + this->request; // assemble new uri to prevent user/password appear clear
    return 0;
}


void RTSPStream::onPacket(void* data, int dataLen){
    if (this->on_packet_callback)
        this->on_packet_callback(data, dataLen, this->aux);
}

void RTSPStream::onClose(){
    //if (this->on_close_callback)
    //    this->on_close_callback(this->aux);
}

void RTSPStream::setOnPacketCallback(std::function<void(void*, int, void*)>on_packet_callback){
    this->on_packet_callback = on_packet_callback;
}

void RTSPStream::setOnCloseCallback(std::function<void(void*)>on_close_callback){
    this->on_close_callback = on_close_callback;
}

void RTSPStream::setOnOpenCallback(std::function<void(void*)>on_open_callback){
    this->on_open_callback = on_open_callback;
    
}

void RTSPStream::onTCPOpen(int socket, void* arg){
    TCPClient* client = (TCPClient*) arg;
    RTSPStream* self = (RTSPStream*) client->auxilary;
    self->state = STATE_OPTIONS;
    string s = self->buildCommand("OPTIONS", "");
    self->client.send(s);
    
    //if (self->on_open_callback)
    //    self->on_open_callback(self->aux);
    
}
int RTSPStream::checkAuth(string str){
    int start, end;
    
    if (str.find("WWW-Authenticate:") != string::npos){
        if (str.find(" Basic ") != string::npos) { // basic auth
            this->authType = "Authorization: Basic "+base64_encode(this->username+":"+this->password, false)+"\r\n";
        } else
        if (str.find(" Digest ") != string::npos) { // digest auth
            start = str.find("realm");
            start = str.find("\"", start);
            end = str.find("\"", start+1);
            this->realm = str.substr(start+1, end - start - 1);

            start = str.find("nonce");
            start = str.find("\"", start);
            end = str.find("\"", start+1);
            this->nonce = str.substr(start+1, end - start - 1);
            this->authType = "digest";                        
        } else{
            logMessage(LOGGER_ERROR, "Unknown auth type");
            return -1;
        }
    } else
        logMessage(LOGGER_DEBUG, "no auth");
    
    this->authenticated = true;
    return 0;
}

void RTSPStream::onTCPData(void* data, int dataLen, void* arg){
    TCPClient* client = (TCPClient*) arg;
    RTSPStream* self = (RTSPStream*) client->auxilary;
    string str;
    int start, end;
    string s;
    
    str.assign((char*)data, dataLen);
    if (str.find("200 OK") != string::npos){
        self->uriMod = "";
        switch(self->state){
            case STATE_OPTIONS:
                s = self->buildCommand("DESCRIBE", "");
                self->client.send(s);
            break;

            case STATE_DESCRIBE:
                if (str.find("200 OK") != string::npos){
                    start = str.find("\r\n\r\n");
                    str = str.substr(start+4); // take payload, skip header
                    self->sdp = str;

                WORD p;
                p = self->udpCh1.init(self->localPortCh1, "0.0.0.0", self);
                if ( p == 0){
                    cout << "Cant find free UDP port pair" << endl;
                    self->client.closeSocket();
                    return;
                }
                if (self->localPortCh1 == 0)
                    self->localPortCh1 = p;
                self->localPortCh2 = self->localPortCh1 + 1;
                self->udpCh2.init(self->localPortCh2, "0.0.0.0", self);

                    // need to extract SSID and FPS
                    self->uriMod = "/trackID=0";
                    s = self->buildCommand("SETUP", "Transport: RTP/AVP;unicast;client_port="+ to_string(self->localPortCh1) + "-" + to_string(self->localPortCh2));
                    self->client.send(s);
                }                
            break;

            case STATE_SETUP:
                start = str.find("server_port");
                if (start != string::npos){
                    start = str.find("=", start);
                    start++;
                    end = str.find("-", start);
                    string t = str.substr(start, end-start);
                    try{
                        self->portCh1 = stoi(t, nullptr, 0);
                    } catch (...){
                        self->portCh1 = 554;
                    }
                    self->portCh2 = self->portCh1+1;

                }
                self->streamReady();

                self->udpCh1.setDataHandlerCallback(self->udpCh1Receiver);
                self->udpCh2.setDataHandlerCallback(self->udpCh2Receiver);
                cout << "sending primer to ports: " << std::to_string(self->portCh1) << " - " << std::to_string(self->portCh2) << endl;
                self->udpCh1.sendTo(self->primer,4,self->ip,self->portCh1);
                self->udpCh2.sendTo(self->primer,4,self->ip,self->portCh2);
                self->udpCh1.sendTo(self->primer,4,self->ip,self->portCh1);
                self->udpCh2.sendTo(self->primer,4,self->ip,self->portCh2);



                start = str.find("Session:");
                if (start != string::npos){
                    start = start+9;
                    end = str.find(";", start);
                    string session = str.substr(start, end-start);
                    s = self->buildCommand("PLAY", "Session: "+session+"\r\nRange: npt=0.000-");
                    self->client.send(s);
                }
            break;


        }
        self->state++;
    } else
    if (!self->authenticated){
        if (self->checkAuth(str) < 0){
            self->client.closeSocket();
        } else {
            string s = self->buildCommand("OPTIONS", "");
            self->client.send(s);
        }
    } else
        self->client.closeSocket();
}

void RTSPStream::onTCPClose(string id, void* arg){
    TCPClient* client = (TCPClient*) arg;
    RTSPStream* self = (RTSPStream*) client->auxilary;
    if (self->on_close_callback)
        self->on_close_callback(self->aux);
}

void RTSPStream::streamReady(){
    if (this->on_open_callback)
        this->on_open_callback(this->aux);

}

