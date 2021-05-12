/* 
 * File:   H264Stream.cpp
 * Author: Dmitry Shurbin
 *
 * Created on May 12, 2021, 5:39 PM
 * This is the class which consumes raw h264 RTP stream and produces 
 * h.264 frames
 */

#include "H264Stream.h"


H264Stream::H264Stream(){
    this->on_packet_callback = NULL;
    this->partList.clear();
}

H264Stream::~H264Stream(){
    this->on_packet_callback = NULL;
    struct h264_part_t* item;
    for (list<h264_part_t*>::iterator it = this->partList.begin(); it != this->partList.end(); it++){
        item = *it;
        if ((item->nBytes > 0) && (item->data != NULL)){
            free(item->data);
            item->nBytes = 0;
        }
        delete item;
    }
    
    this->partList.clear();
}


void H264Stream::setOnPacketCallback(std::function<void(void*, int, void*)>on_packet_callback){
    this->on_packet_callback = on_packet_callback;
}
void H264Stream::init(void* arg){
    this->aux = arg;
}

void H264Stream::rawRTPConsumer(void* data, int nBytes, void* arg){
    H264Stream* self = (H264Stream*) arg;
    
    uint8_t* data8 = (uint8_t*) data;
    struct h264_part_t* item;
    
    if (nBytes > 12){
        item = new h264_part_t;
        item->seq = (data8[2] << 8) + data8[3];
        item->timeStamp = (data8[4] << 24) + (data8[5] << 16) + (data8[6] << 8) + data8[7];
        if ((data8[1] & 0x80) != 0)
            item->mark = true;
        else
            item->mark = false;
        item->nBytes = nBytes - 12;
        item->data = (uint8_t*)malloc(item->nBytes);
        memcpy(item->data, data8+12, item->nBytes);
        self->partList.push_back(item);
    
        if (item->mark){ // frame finished
            // sort parts
            self->partList.sort(compare_parts());
            uint8_t* frame = NULL;
            int frameSize = 0;
            for (list<h264_part_t*>::iterator it = self->partList.begin(); it != self->partList.end(); it++){
                item = *it;
                uint8_t FUIndicator;
                uint8_t FUHeader;
                FUIndicator = item->data[0];
                FUHeader = item->data[1];
                if ((FUIndicator == 0x06) || (FUIndicator == 0x68) || (FUIndicator == 0x67)){
                    
                    uint8_t *buffer = NULL;
                    buffer = (uint8_t*)malloc(item->nBytes + sizeof(self->H264_FRAME_HEADER));
                    memcpy(buffer, self->H264_FRAME_HEADER, sizeof(self->H264_FRAME_HEADER));
                    memcpy(buffer + sizeof(self->H264_FRAME_HEADER), item->data, item->nBytes);
                    if (self->on_packet_callback)
                        self->on_packet_callback(buffer, item->nBytes + sizeof(self->H264_FRAME_HEADER), self->aux);
                    free(buffer);
                } else {
                    if (frame == NULL){ // first part
                        frame = (uint8_t*)malloc(item->nBytes-1);
                        uint8_t frameType;
                        frameType = (FUIndicator & 0xE0) | (FUHeader & 0x1F);
                        item->data[1] = frameType;
                        memcpy(frame, item->data+1, item->nBytes-1);
                        frameSize = item->nBytes-1;
                    } else {
                        uint8_t* tmpFrame;
                        tmpFrame = (uint8_t*)realloc(frame, frameSize + (item->nBytes - 2));
                        if (tmpFrame != NULL){
                            frame = tmpFrame;
                            memcpy(frame+frameSize, item->data+2, item->nBytes - 2);
                            frameSize += item->nBytes - 2;
                        }
                    }
                }

                free(item->data);
                item->data = NULL;
                item->nBytes = 0;
                delete item;
            }

            self->partList.clear();
            uint8_t *buffer = NULL;
            buffer = (uint8_t*)malloc(frameSize + sizeof(self->H264_FRAME_HEADER));
            memcpy(buffer, self->H264_FRAME_HEADER, sizeof(self->H264_FRAME_HEADER));
            memcpy(buffer + sizeof(self->H264_FRAME_HEADER), frame, frameSize);
            free(frame);
            
            if (self->on_packet_callback)
                self->on_packet_callback(buffer, frameSize + sizeof(self->H264_FRAME_HEADER), self->aux);
            
            frameSize = 0;
            free(buffer);
        }
    }


}