#include "sound.h"

int main() {
    using namespace std;
    Mp3 track("./materials/sad.mp3");
    cout << track.trackLength();
}

const size_t* tableScaleBandBoundary(size_t n) {
    switch (n) {
        case (44100):
            return tfreq44100;
        case (48000):
            return tfreq48000;
        case (32000):
            return tfreq32000;
        default:
            throw "...";
    }              
}                  

double mp3FloatRep1(size_t n) {
    return pow(2.0, 0.25 * (n - 210));
} 
double mp3FloatRep2(size_t n) {
    return pow(2.0, 0.25 * (-n * 8));
} 

BlockFlag toBlockFlag(bool mixedFlag, size_t blockType) {
    if (mixedFlag) return BlockFlag::MixedBlocks;
    if (blockType == 2) return BlockFlag::ShortBlocks;
    return BlockFlag::LongBlocks;
}

SideInfo::SideInfo(std::istream& f, FrameHeader& header) {
    using namespace std;
    BitStream bs {f};
    if (header.mono()) {
        auto dataptr     = bs.getBits(9);
        auto privateBit  = bs.getBits(5); 
        scfsi            = bs.getBits(4); 
        auto granule0ch0 = SideData(bs, header);
        auto granule1ch0 = SideData(bs, header);
        granules = make_tuple(granule0ch0, granule1ch0);
    }
    else {
        auto dataptr     = bs.getBits(9); 
        auto privateBit  = bs.getBits(3); 
        scfsi            = bs.getBits(8); 
        auto granule0ch0 = SideData(bs, header);
        auto granule1ch0 = SideData(bs, header);
        auto granule0ch1 = SideData(bs, header);
        auto granule1ch1 = SideData(bs, header);
        granules = make_tuple(granule0ch0, granule1ch0, granule0ch1, granule1ch1);
    }
}

inline bool validateTable(size_t n) {
    if (n == 4 || n == 14) return false;
    return true;
}

SideData::SideData(BitStream& bs, FrameHeader& header) {
    using namespace std;
    auto part23         = bs.getBits(12);
    auto bigvalues      = bs.getBits(9);
    auto globalgainb    = bs.getBits(8);
    auto globalgain     = mp3FloatRep1(globalgainb);
    auto scalelengths   = tableSlen[bs.getBits(4)];
    auto flag           = bs.getBits(1);
    auto blocktype      = bs.getBits(flag ? 2 : 0);
    auto mixed          = bs.getBits(flag ? 1 : 0);
    auto blockflag      = toBlockFlag(mixed, blocktype);
    auto table0         = bs.getBits(5);
    auto table1         = bs.getBits(5);
    auto table2         = bs.getBits(flag ? 0 : 5);
    auto subgain0b      = bs.getBits(flag ? 3 : 0);
    auto subgain1b      = bs.getBits(flag ? 3 : 0);
    auto subgain2b      = bs.getBits(flag ? 3 : 0);
    auto subgain0       = mp3FloatRep2(subgain0b);
    auto subgain1       = mp3FloatRep2(subgain1b);
    auto subgain2       = mp3FloatRep2(subgain2b);
    auto regionabits         = bs.getBits(flag ? 0 : 4);
    auto regionbbits         = bs.getBits(flag ? 0 : 3);
    size_t racnt{}, rbcnt{};
    if (flag) {
        if (blocktype == 2) racnt = 8;
        else racnt = 7;
        rbcnt = 20 - racnt;
    }
    else {
        racnt = regionabits;
        rbcnt = regionbbits;
    }
    auto sbTable   = tableScaleBandBoundary(header.sampling_rate);
    auto r1bound   = sbTable[racnt + 1];
    auto r2bound   = sbTable[racnt + 1 + rbcnt + 1];
    auto bv2       = bigvalues*2;
    size_t r0len{}, r1len{}, r2len{};
    if (blocktype == 2) {
        r0len = min(bv2, size_t{36}); 
        r1len = min(bv2 - r0len, size_t{540});
        r2len = 0;
    }
    else {
        r0len = min(bv2, r1bound);
        r1len = min(bv2 - r0len, r2bound - r0len);
        r2len = bv2 - (r0len + r1len);
    }
    auto preflag = bs.getBits(1);
    auto scalefacbit = bs.getBits(1);
    auto scalefacscale = scalefacbit;
    auto count1table = bs.getBits(1);
    HuffmanData huffdata {bigvalues, make_tuple(r0len, r1len, r2len),
        make_tuple(table0, table1, table2), count1table};
    ScaleData scaledata {globalgain, make_tuple(scalelengths[0], scalelengths[1]), make_tuple(subgain0, subgain1, subgain2),
        scalefacscale, preflag};
    if (validateTable(table0) && validateTable(table1) && (flag || validateTable(table2))) {
       sideHuffman = new HuffmanData(move(huffdata));
       sideScalefactor = new ScaleData(move(scaledata));
       sidePart23Length = part23;
       sideBlocktype = blocktype;
       sideBlockflag = blockflag;
    }
    else {
        throw "SideInfo::SideInfo()";
    }
}

SideData::SideData() { }

BitStream::BitStream(std::istream& is) : is{is} { }
inline size_t BitStream::inc() {
    return (++i) == 8 ? (i = 0, ++y) : 0;
}
size_t BitStream::getBits(size_t n) {
    size_t res{};
    size_t idx{1};
    while (y < buf.size() && n) {
        res |= (buf[y] & (8 - i));
        inc(), n -= 1;
    } 
    if (n > 0) {
        size_t sz {n/8 + 1};
        if (buf.size() < sz) buf.resize(sz);
        is >> buf;
        y = i = 0;
        while (n) {
            res |= (buf[y] & (8 - i));
            inc(), n -= 1;
        }
    }
    return res;
}

bool FrameHeader::mono() {
    return channel_mode == ChannelMode::Mono;
}


template<typename T>
inline size_t toInt(std::vector<T>& v) {
    using namespace std;
    size_t res{};
    if (is_same<T, char>::value) {
        for (size_t i{}; i < v.size(); ++i) {
            res = res | ((uint8_t)v[i] << ((v.size()-1-i)*8));
        } 
    }
    return res;
}

std::ostream& operator<<(std::ostream& os, ChannelMode& cm) {
    switch (cm) {
        case(ChannelMode::Mono): {
            os << "Mono";
            break;
        } 
        case(ChannelMode::Stereo): {
            os << "Stereo";
            break;
        } 
        case(ChannelMode::DualChannel): {
            os << "DualChannel";
            break;
        } 
        case(ChannelMode::JointStereo): {
            os << "JointStereo";
            break;
        } 
    }
    return os;
}

bool operator==(std::vector<char>& v, const char* s) {
    for (size_t i{}; i < v.size(); ++i) {
        if (v[i] != s[i]) return false;
    }
    return true;
}

bool operator!=(std::vector<char>& v, const char* s) {
    for (size_t i{}; i < v.size(); ++i) {
        if (v[i] != s[i]) return true;
    }
    return false;
}

template<std::size_t N>
void reverse(std::bitset<N> &b) {
    for(std::size_t i = 0; i < N/2; ++i) {
        bool t = b[i];
        b[i] = b[N-i-1];
        b[N-i-1] = t;
    }
    
}

template <typename T>
std::istream& operator>>(std::istream& is, std::vector<T>& v) {
    is.read(&v[0], v.size());
    return is;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, std::vector<T>& v) {
    for (auto& e : v) os << e;
    return os;
}

template <typename Cnt>
inline size_t bitSub(Cnt bs, size_t from, size_t to) {
    size_t res{};
    for (size_t i{1};from <= to; ++from) {
        res += bs[from] * i; 
        i <<= 1;
    }
    return res;
}

FrameHeader::FrameHeader(std::istream& f) {
    using namespace std;
    vector<char> frameBytes(4);
    f >> frameBytes;
    frame = 0;
    for (size_t i{}; i < 4; ++i) {
        frame = frame | ((uint8_t)frameBytes[i] << ((3-i)*8));
    } 
    bitset<32> bs {frame};
    reverse(bs);
    cout << "FRAME BITS: ";
    for (int i = 0; i < 32; ++i) {
        cout << bs[i];
    }
    cout << endl;
    mpeg = fH::MPEG[bitSub(bs, 18, 19)];
    layer = fH::Layer[bitSub(bs, 16, 17)];
    bitrate = fH::bitrate[2][bitSub(bs, 11, 14) - 1];
    sampling_rate = fH::sampling_rate[0][bitSub(bs, 21, 22)];
    padding_bit = bs[23];
    channel_mode  = fH::channel_mode[bitSub(bs, 25, 26)];
    auto flb = (144 * 1000 * bitrate)/sampling_rate + padding_bit;
}

size_t FrameHeader::length() {
    return (144 * 1000 * bitrate)/sampling_rate + padding_bit;
}


size_t Mp3::trackLength() {
    using namespace std;
    if (compType == Compression::CBR) {
        return (fsize - tagLen)/(fh->bitrate*1000) * 8;
    }   
    else {
        return fh->frameCount*1152/fh->sampling_rate;
        return 0;
    }
}

Mp3::Mp3(const char* fn) {
    using namespace std;
    fsize = filesystem::file_size(fn);
    f.open(fn, ios::binary);
    DEBUG(fsize);
    vector<char> marker(3);
    f >> marker;
    if (marker != "ID3") {
        cerr << "not an id3v2?" << endl;
    }
    f.seekg(ID3v2::length);
    vector<char> lenBuf(4);
    f >> lenBuf; tagLen = 0;
    for (size_t i{}; i < 4; ++i) {
        tagLen += (lenBuf[i] & 0x7f) << (7*(3-i));
    }
    f.seekg(tagLen + ID3v2::finish);
    fh = new FrameHeader(f); 
    DEBUG(marker);
    DEBUG(tagLen);
    DEBUG(fh->frame);
    DEBUG(fh->mpeg);
    DEBUG(fh->layer);
    DEBUG(fh->bitrate);
    DEBUG(fh->sampling_rate);
    DEBUG(fh->padding_bit);
    DEBUG(fh->channel_mode);
    size_t xingOff = fh->channel_mode != ChannelMode::Mono ? 32 : 17;
    DEBUG(xingOff);
    vector<char> sign(4);
    f.seekg(tagLen + ID3v2::finish + xingOff);
    f >> sign;
    fh->frameCount = 0;
    if (sign == "Xing") {
        compType = Compression::XING;
        f.seekg(4, ios_base::cur);
        vector<char> scBytes(4);
        f >> scBytes;
        for (size_t i{}; i < 4; ++i) {
            fh->frameCount = fh->frameCount | ((uint8_t)scBytes[i] << ((3-i)*8));
        } 
    }
    else {
        if (xingOff != 32) {
            f.seekg(tagLen + ID3v2::finish + 32);
            f >> sign;
        }
        if (sign == "VBRI") {
            compType = Compression::VBRI;
            f.seekg(10, ios_base::cur);
            vector<char> scBytes(3);
            f >> scBytes;
            for (size_t i{}; i < 4; ++i) {
                fh->frameCount = fh->frameCount | ((uint8_t)scBytes[i] << ((3-i)*8));
            } 
        }
        else compType = Compression::CBR;
    }
    DEBUG(fh->frameCount);
    DEBUG((int)compType);
}

Mp3::~Mp3() {
    f.close();
}
