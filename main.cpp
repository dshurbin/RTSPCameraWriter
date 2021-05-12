/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.cpp
 * Author: dmitry
 *
 * Created on May 11, 2021, 7:27 PM
 */

#include <cstdlib>
#include <list>
#include <algorithm>
#include "RTSPStream.h"
#include "H264Stream.h"

using namespace std;
FILE* f;
int n=0;


static void on_open_callback(void* arg){
    printf("Stream opened\n");
}

static void on_close_callback(void* arg){
    printf("=============Stream closed=====================\n");
}
static void on_packet_callback(void* data, int nBytes, void* arg){
    fwrite(data, nBytes, 1, f);
    fflush(f);

}

/*
 * 
 */
int main(int argc, char** argv) {
    loggerLevel = LOGGER_INFO | LOGGER_WARNING | LOGGER_DEBUG | LOGGER_ERROR;
    RTSPStream camera;
    H264Stream h264;
    
    printf("RTSP stream writer v1.0a\n");
    if (argc == 3){
        string uri;
        uri.assign(argv[1], strlen(argv[1]));
        printf("camera: %s file: %s\n" , uri.c_str(), argv[2]);
        
        f = fopen(argv[2], "wb");
        camera.setOnOpenCallback(on_open_callback);
        camera.setOnCloseCallback(on_close_callback);
        h264.init(NULL);
        
        camera.setOnPacketCallback(h264.rawRTPConsumer);
        h264.setOnPacketCallback(on_packet_callback);
        camera.open(uri, &h264);

        printf("Press Ctrl-C to quit\n");
        for(;;){
            usleep(10000);
        }
        camera.close();
        printf("Done.\n");
    } else {
        printf("Usage: %s <rtsp-uri> <destination file>\n", argv[0]);
        printf("argc = %d\n", argc);
    }
    return 0;
}

