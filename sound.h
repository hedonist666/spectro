#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdint>
struct Mp3 {
    std::FILE* f;
    char* marker;
    size_t length = 0;
    Mp3(const char*);
    ~Mp3();
};

namespace frameData {
    using namespace std;
    using T = size_t; //SHIT!!! THATS'S NOT BYTES, THATS BITS!!!!
    T marker = 0;
    T version_index = 11;
    T layer_index = 13;
    T protection_bit = 15;
    T bitrate_index = 16;
    T frequency_index = 20;
    T offset_bit = 22;
    T channel_mode_index = 24;
    T extend_mode = 26;
    T copyright = 28;
    T original = 29;
    T accent = 30; 
    const char* MPEG[] = 
        {"MPEG-2.5", "not used", "MPEG-2", "MPEG-1"};
    const char* Layer[] =
        {"not used", "Layer III", "Layer II", "Layer I"};
    int bitrate[][14] =
        {{32,64,96,128,160,192,224,256,288,320,352,384,416,448}, 
         {32,48,64,56,80,96,112,128,160,192,224,256,320,384},
         {32,40,48,56,64,80,96,112,128,160,192,224,256,320},
         {32,48,56,64,80,96,112,128,144,160,176,192,224,256},
         {8,16,24,32,40,48,56,64,80,96,112,128,144,160}};
    int sampling_rate[][3] =
        {{44100, 48000, 32000},
         {22050, 24000, 16000},
         {11025, 12000, 8000}};
    const char* channel_mode[] =
        {"Stereo", "Joint stereo", "Dual channel", "Mono"};
};

namespace ID3v2 {
    size_t marker = 0;
    size_t version = 3;
    size_t subVersion = 4;
    size_t flags = 5;
    size_t length = 6;
    size_t finish = 10;
};

namespace ID3v1 {
    size_t header    = 0;
    size_t title     = 3;
    size_t artist    = 33;
    size_t album     = 63;
    size_t year      = 93;
    size_t comment   = 97;
    size_t zero_byte = 125;
    size_t track     = 126;
    size_t genre     = 127;
};

#define DEBUG(x) cout << #x << ": "<< x << endl;
#define BIT(x, n) ((x & (1 << n)) ? 1 : 0)
