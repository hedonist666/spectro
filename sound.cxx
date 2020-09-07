#include "sound.h"

int main() {
    using namespace std;
    Mp3 track("./materials/sad.mp3");
    cout << track.trackLength();
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

FrameHeader::FrameHeader(std::ifstream& f) {
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
    auto flb = 144 * bitrate/(sampling_rate + padding_bit);
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
