/* 
 * File:   H264Stream.h
 * Author: Dmitry Shurbin
 *
 * Created on May 12, 2021, 5:39 PM
 * This is the class which consumes raw h264 RTP stream and produces 
 * h.264 frames
 */

#ifndef H264STREAM_H
#define H264STREAM_H

#include <list>
#include <algorithm>
#include <functional>
#include <cstring>
#include <arpa/inet.h>
#include "Logger.h"

using namespace std;

struct h264_part_t{
    uint16_t seq;
    uint32_t timeStamp;
    bool mark;
    int nBytes;
    uint8_t* data;
};
struct compare_parts {
    bool operator() (h264_part_t const* left, h264_part_t const* right) {
        return left->seq < right->seq;
    }
};


class H264Stream{
    public:
        H264Stream();
        ~H264Stream();
        void init(void* arg);
        static void rawRTPConsumer(void* data, int nBytes, void* arg);
        void setOnPacketCallback(std::function<void(void*, int, void*)>);
    
    protected:
        void* aux;
        const uint8_t H264_FRAME_HEADER[4] = {0,0,0,1};
        list<h264_part_t*> partList;
        std::function<void(void*, int, void*)> on_packet_callback;

};

#endif /* H264STREAM_H */

