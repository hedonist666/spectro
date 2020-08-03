#include "sound.h"

int main() {
    using namespace std;
    Mp3 track("./materials/ball.mp3");
    cout << track.trackLength();
}

size_t Mp3::trackLength() {
    using namespace std;
    if (compType == Compression::CBR) {
        return (fsize - length)/(bitrate*1000) * 8;
    }   
    else {
        return frameCount*1152/sampling_rate;
        return 0;
    }
}
Mp3::Mp3(const char* fn) {
    using namespace std;
    f = fopen(fn, "r");
    fsize = filesystem::file_size(fn);
    DEBUG(fsize);
    marker = new char[3];
    fread(marker, 1, 3, f); 
    if (strcmp(marker, "ID3")) {
        cout << "not an id3v2?" << endl;
    }
    DEBUG(marker);
    fseek(f, ID3v2::length, SEEK_SET); 
    char* lenBuf = new char[4]; 
    fread(lenBuf, 1, 4, f);
    for (size_t i{}; i < 4; ++i) {
        length += (lenBuf[i] & 0x7f) << (7*(3-i));
    }
    DEBUG(length);
    delete[] lenBuf;
    fseek(f, length + ID3v2::finish, SEEK_SET);
    char* frameBytes = new char[4];
    fread(frameBytes, 1, 4, f);
    for (size_t i{}; i < 4; ++i) {
        frame = frame | ((uint8_t)frameBytes[i] << ((3-i)*8));
    } 
    DEBUG(frame);
    cout << "FRAME BITS: " << endl;
    for (int i = 0; i < 32; ++i) {
        cout << BIT(frame, (31 - i));
    }
    cout << endl;
    mpeg = frameData::MPEG[2*BIT(frame, 20) + BIT(frame, 19)];
    DEBUG(mpeg);
    if (strcmp(mpeg, "MPEG-1")) {
        cout << "not 1?" << endl;
    }
    layer = frameData::Layer[2*BIT(frame, 18) + BIT(frame, 17)];
    DEBUG(layer);
    if (strcmp(layer, "Layer III")) {
        cout << "not III?" << endl;
    } 
    bitrate = frameData::bitrate[2][8*BIT(frame, 15) + 4*BIT(frame, 14) + 2*BIT(frame, 13) + BIT(frame, 12) - 1];
    DEBUG(bitrate);
    sampling_rate = frameData::sampling_rate[0][2*BIT(frame, 11) + BIT(frame, 10)];
    DEBUG(sampling_rate);
    padding_bit = BIT(frame, 9);
    DEBUG(padding_bit);
    channel_mode  = frameData::channel_mode[2*BIT(frame, 7) + BIT(frame, 6)];
    DEBUG(channel_mode);
    size_t xingOff = strcmp(channel_mode, "Mono") ? 32 : 17;
    DEBUG(xingOff);
    char* sign = new char[4];
    fseek(f, length + ID3v2::finish + xingOff, SEEK_SET);
    fread(sign, 1, 4, f);
    frameCount = 0;
    if (!strcmp(sign, "Xing")) {
        compType = Compression::XING;
        fseek(f, 4, SEEK_CUR);
        char* scBytes = new char[4];
        fread(scBytes, 1, 4, f);
        for (size_t i{}; i < 4; ++i) {
            frameCount = frameCount | ((uint8_t)scBytes[i] << ((3-i)*8));
        } 
        delete[] scBytes;
    }
    else {
        if (xingOff != 32) {
            fseek(f, length + ID3v2::finish + 32, SEEK_SET);
            fread(sign, 1, 4, f);
        }
        if (!strcmp(sign, "VBRI")) {
            compType = Compression::VBRI;
            fseek(f, 10, SEEK_CUR);
            char* scBytes = new char[4];
            fread(scBytes, 1, 4, f);
            for (size_t i{}; i < 4; ++i) {
                frameCount = frameCount | ((uint8_t)scBytes[i] << ((3-i)*8));
            } 
        }
        else compType = Compression::CBR;
    }
    DEBUG(frameCount);
    delete[] sign;
    DEBUG((int)compType);
}

Mp3::~Mp3() {
    delete[] marker;
    fclose(f);
}
