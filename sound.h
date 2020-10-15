#include <iostream>
#include <optional>
#include <vector>
#include <string>
#include <cstdio>
#include <variant>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <bitset>
#include <tuple>
#include <iterator>
#include <algorithm>

//#include "table.h"

enum class Compression {
    CBR = 0,
    XING,
    VBRI
};

enum class ChannelMode {
    Mono = 0,
    Stereo,
    DualChannel,
    JointStereo
};

enum class BlockFlag {
    LongBlocks,
    ShortBlocks, 
    MixedBlocks 
};

//left - 0, right - 1
namespace Huffman {
    template <typename A>
    struct Tree {
        union {
            A a;
            std::pair<Tree<A>*, Tree<A>*> b;
        };
        enum class State {
            EMPTY,
            NODE,
            LEAF
        } state;
        Tree();
        void insert(bool*, size_t, A);
        std::optional<std::pair<A, size_t>> lookup(bool*, size_t n = 0);
        template<typename Getter>
        std::optional<std::pair<A, size_t>> lookup(Getter, size_t n = 0);
    };
};

BlockFlag toBlockFlag(bool mixedFlag, size_t blockType);

struct BitStream {
    size_t i{};
    size_t y{};
    std::vector<char> buf;
    std::istream& is;
    BitStream(std::istream& is);
    size_t getBits(size_t);
    size_t lookAhead(size_t);
    std::vector<char> getByteString(size_t);
    std::vector<char> lookAheadBytes(size_t);
    bool get();
    inline size_t inc();
};

struct FrameHeader {
    std::string mpeg;
    std::string layer;
    uint32_t frame;
    size_t bitrate;
    size_t sampling_rate;
    uint8_t padding_bit;
    ChannelMode channel_mode;
    size_t frameCount;
    bool is, ms;
    bool crc;
    FrameHeader(uint32_t frame);
    size_t length();
    bool mono();
    bool sane();
};

struct HuffmanData {
    BitStream& bs;
    size_t bigVal;
    std::tuple<size_t, size_t, size_t> region_len;
    std::tuple<size_t, size_t, size_t> table;
    size_t count1table;
    //TODO
    std::vector<size_t> decode(size_t);
    std::pair<std::vector<size_t>, size_t> decodeRegion(size_t, size_t);
    std::pair<std::pair<size_t, size_t>, size_t> decodeOne(size_t);
    std::pair<std::tuple<size_t, size_t, size_t, size_t>, size_t> decodeOneQuad(size_t);
    std::vector<size_t>& decodeRegionQ(size_t, size_t, std::vector<size_t>&);
};

struct ScaleFactors {
    std::vector<size_t> a;
    std::vector<std::vector<size_t>> b;
    size_t c;
};

struct Scale {
    std::vector<size_t> scaleL;
    std::vector<std::vector<size_t>> scaleS; 
    size_t bitsread;
};

struct ScaleData {
    double scaleGlobalGain;
    std::pair<size_t, size_t> slen;
    std::tuple<double, double, double> scaleSubblockGain;
    size_t scalefac;
    size_t scalePreflag;
};


struct SideData {
    HuffmanData* sideHuffman;
    ScaleData* sideScalefactor;
    size_t sidePart23Length;
    size_t sideBlocktype;
    BlockFlag sideBlockflag;
    SideData(BitStream&, FrameHeader&);
    SideData();
    Scale parseRawScaleFactors(BitStream&, size_t, const std::vector<size_t>&);
    std::pair<std::vector<double>, std::vector<std::vector<double>>> unpackScaleFactors(std::vector<size_t>,
            std::vector<std::vector<size_t>>);
};

struct SideInfo {
    size_t mainData;
    size_t scfsi;
    std::variant<std::tuple<SideData, SideData>,
        std::tuple<SideData, SideData, SideData, SideData>> granules;
    /*sideGranule0ch0, sideGranule1ch0, sideGranule0ch0, sideGranule0ch1, 
        sideGranule1ch0, sideGranule1ch1, */ 
    SideInfo(BitStream&, FrameHeader&);
};

struct LogicalFrame {
    FrameHeader* fh;
    SideInfo* si;
    char* data;
};

struct MP3DataChunk {
    size_t chunkBlockType;
    BlockFlag chunkBlockFlag;
    double chunkScaleGain;
    std::tuple<double, double, double> chunkScaleSubGain;
    std::vector<double> chunkScaleLong;
    std::vector<std::vector<double>> chunkScaleShort;
    std::pair<std::vector<size_t>, std::vector<std::vector<size_t>>> chunkISParam;
    std::vector<size_t> chunkData;
};

struct MP3Data {
    size_t sampleRate;
    ChannelMode channelMode;
    std::pair<bool, bool> someFlags;
    std::variant<
        std::tuple<MP3DataChunk, MP3DataChunk>,
        std::tuple<MP3DataChunk, MP3DataChunk, MP3DataChunk, MP3DataChunk>
    > ch;
};

LogicalFrame unpackFrame(BitStream& bs);
MP3Data parseMainData(BitStream& bs, LogicalFrame& lf);


struct Mp3 {
    size_t fsize;
    size_t tagLen;
    BitStream bs;
    LogicalFrame* lf;
    std::ifstream f;
    Compression compType;
    Mp3(const char*);
    size_t trackLength();
    MP3Data unpackOne();
    ~Mp3();
};

namespace fH {
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
    T last_bit = 31;
    T finish = 32;
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
   ChannelMode channel_mode[] =
        {ChannelMode::Stereo, ChannelMode::JointStereo, 
            ChannelMode::DualChannel, ChannelMode::Mono};
    int samples_per_frame[][3] = 
        {{384, 1152, 1152},
         {384, 1152, 576},
         {384, 1152, 576}};
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


size_t tableSlen[][2] = {{0,0}, {0,1}, {0,2}, {0,3}, {3,0}, {1,1}, {1,2}, {1,3},
             {2,1}, {2,2}, {2,3}, {3,1} ,{3,2}, {3,3}, {4,2}, {4,3}};


std::vector<size_t> tablePretab {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
               1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0, 0};


const size_t* tableScaleBandBoundary(size_t);

const size_t tfreq44100[] = {0,4,8,12,16,20,24,30,36,44,52,62,74,90,110,134,162,196,238,288,342,418,576};
const size_t tfreq48000[] ={0,4,8,12,16,20,24,30, 36,42,50,60,72,88,106,128, 156,190,230,276,330,384,576};
const size_t tfreq32000[] ={0,4,8,12,16,20,24,30, 36,44,54,66,82,102,126,156, 194,240,296,364,448,550,576};

double mp3FloatRep1(size_t);
double mp3FloatRep2(size_t);
inline bool validateTable(size_t);

std::ostream& operator<<(std::ostream& os, ChannelMode& cm);
bool operator==(std::vector<char>& v, const char* s); 
bool operator!=(std::vector<char>& v, const char* s);
template<std::size_t N>
void reverse(std::bitset<N> &b);
template <typename T>
std::istream& operator>>(std::istream& is, std::vector<T>& v);
template <typename T>
std::ostream& operator<<(std::ostream& os, std::vector<T>& v);
template <typename Cnt>
inline size_t bitSub(Cnt bs, size_t from, size_t to);
template<typename T>
inline size_t toInt(std::vector<T>&);
std::pair<Huffman::Tree<std::pair<size_t, size_t>>, size_t> huffmanDecodeTable(size_t);
Huffman::Tree<std::tuple<size_t, size_t, size_t, size_t>> huffmanDecodeTableQuad(size_t);

template <typename CharT, typename Traits = std::char_traits<CharT>>
struct vectorwrap : public std::basic_streambuf<CharT, Traits> {
    vectorwrap(std::vector<CharT>& v) {
        setg(v.data(), v.data() + v.size());
    }
};




#define DEBUG(x) cout << #x << ": "<< x << endl;
#define BIT(x, n) (((x) & (1 << (n))) ? 1 : 0)

