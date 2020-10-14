#include "sound.h"


int main() {
    using namespace std;
    Mp3 track("./materials/sad.mp3");
    cout << track.trackLength();
}


inline std::pair<std::vector<size_t>, MP3DataChunk>
helperMD(BitStream& bs, SideData& side, size_t scfsi, const std::vector<size_t>& gran) {
    if (!side.sideHuffman) throw "parseMainData:: empty huffman";
    if (!side.sideScalefactor) throw "parseMainData:: empty huffman";
    //надеюсь компилятор отметет эти присвоения
    auto& huffdata = *side.sideHuffman;
    auto& scaledata = *side.sideScalefactor;
    auto bt = side.sideBlocktype;
    auto bf = side.sideBlockflag;
    auto part23 = side.sidePart23Length;
    auto [scaleL, scaleS, bitsread] = side.parseRawScaleFactors(bs, scfsi, gran);
    auto [scaleFacLarge, scaleFacSmall] = side.unpackScaleFactors(scaleL, scaleS);
    auto maindata = huffdata.decode(part23 - bitsread);
    return {scaleL, {bt, bf, scaledata.scaleGlobalGain, scaledata.scaleSubblockGain, scaleFacLarge,
        scaleFacSmall, {scaleL, scaleS}, maindata}};
}

MP3Data parseMainData(BitStream& bs, LogicalFrame& lf) {
    using namespace std;
    if (!lf.fh) throw "parseMainData:: empty frame header";
    if (!lf.si) throw "parseMainData:: empty side info";
    auto& header = *lf.fh; 
    auto& sideinfo = *lf.si;
    auto scfsi = sideinfo.scfsi;
    auto scfsi0 = scfsi & 0xf0; //возможно надо >> 4
    auto scfsi1 = scfsi & 0xf;
    if (header.mono()) {
        auto [s0s, s0m] = helperMD(bs, get<0>(get<0>(sideinfo.granules)), scfsi, {});
        auto [s1s, s1m] = helperMD(bs, get<1>(get<0>(sideinfo.granules)), scfsi, s0s);
        return {header.sampling_rate, header.channel_mode, {header.ms, header.is}, 
            make_tuple(s0m, s1m)};
    }
    else {
        auto [s00s, s00m] = helperMD(bs, get<0>(get<1>(sideinfo.granules)), scfsi0, {});
        auto [s01s, s01m] = helperMD(bs, get<1>(get<1>(sideinfo.granules)), scfsi1, {});
        auto [s10s, s10m] = helperMD(bs, get<2>(get<1>(sideinfo.granules)), scfsi0, s00s);
        auto [s11s, s11m] = helperMD(bs, get<3>(get<1>(sideinfo.granules)), scfsi1, s01s);
        return {header.sampling_rate, header.channel_mode, {header.ms, header.is}, 
            make_tuple(s00m, s01m, s10m, s11m)};
    }
}


LogicalFrame unpackFrame(BitStream& bs) {
    auto headerBits = bs.getBits(32);    
    FrameHeader* header = new FrameHeader(headerBits);
    size_t lengthcrc = header->crc ? 2 : 0;
    size_t lengthside = header->channel_mode == ChannelMode::Mono ? 17 : 32;
    size_t lengthframe = header->length();
    bs.getBits(32);
    bs.getBits(lengthcrc << 3);
    SideInfo* side = new SideInfo(bs, *header);
    auto main = bs.getBits(lengthframe - 4 - lengthcrc - lengthside); 
    auto peek = bs.lookAhead(8);    
}


std::vector<size_t> HuffmanData::decode(size_t bitlen) {
    using namespace std;
    auto [reg0, bitcount0] = decodeRegion(get<0>(region_len), get<0>(table));
    auto [reg1, bitcount1] = decodeRegion(get<1>(region_len), get<1>(table));
    auto [reg2, bitcount2] = decodeRegion(get<2>(region_len), get<2>(table));
    auto bitsread = bitcount0 + bitcount1 + bitcount2;
    auto rqlen = bitlen - bitsread - 1;
    vector<size_t> accum{};
    auto regQ = decodeRegionQ(count1table, rqlen, accum);
    vector<size_t> res = move(reg0);
    res.reserve(reg0.size() + reg1.size() + reg2.size());
    move(begin(reg1), end(reg1), back_inserter(res));
    move(begin(reg2), end(reg2), back_inserter(res));
    return res;
}

std::pair<std::vector<size_t>, size_t> HuffmanData::decodeRegion(size_t reglen, size_t tablen) {
    using namespace std;
    decltype(decodeRegion(reglen, tablen)) res {};
    for (size_t i{}; i < reglen/2; ++i) {
        auto dec = decodeOne(tablen); 
        res.first.push_back(dec.first.first);
        res.first.push_back(dec.first.second);
        res.second += dec.second;
    }
    return res;
}

std::pair<std::pair<size_t, size_t>, size_t> HuffmanData::decodeOne(size_t n) {
    if (!n) {
        return {{0, 0}, 0};
    }
    auto [table, linbits] = huffmanDecodeTable(n);
    auto mval = table.lookup(bs);
    if (mval) {
        auto [coord, bitn] = mval.value(); 
        auto [x, y] = coord;
        decltype(linbits) bxlin{}, bylin{}, bxsgn{}, bysgn{};
        if (x == 15 && linbits > 0) {
            bxlin = bylin = linbits;
        }
        if (x > 0) {
            bxsgn = bysgn = 1; 
        }
        auto xlin = bs.getBits(bxlin);
        auto xsgn = bs.getBits(bxsgn) ? -1 : 0;
        auto ylin = bs.getBits(bylin);
        auto ysgn = bs.getBits(bysgn) ? -1 : 0;
        auto xx = (x + xlin) * xsgn;
        auto yy = (y + ylin) * ysgn;
        auto bitn2 = bxlin + bylin + bxsgn + bysgn;
        return {{xx, yy}, bitn2};
    }
    throw "...";
}

std::pair<std::tuple<size_t, size_t, size_t, size_t>, size_t> HuffmanData::decodeOneQuad(size_t n) {
    if (!n) {
        return {{0, 0, 0, 0}, 0};
    }
    auto table = huffmanDecodeTableQuad(n);
    auto mval = table.lookup(bs);
    if (mval) {
        auto [coord, bitn] = mval.value(); 
        auto [a, b, c, d] = coord;
#define sb(x) ((x) > 0 ? 1 : 0)
        auto asgn = bs.getBits(sb(a)) ? -1 : 0;   
        auto bsgn = bs.getBits(sb(b)) ? -1 : 0;   
        auto csgn = bs.getBits(sb(c)) ? -1 : 0;   
        auto dsgn = bs.getBits(sb(d)) ? -1 : 0; 
        return {
            {
                a * asgn,
                b * bsgn,
                c * csgn,
                d * dsgn,
            },
            sb(a)+sb(b)+sb(c)+sb(d)+bitn
        };
    }
#undef sb
    else throw "...";
}

std::vector<size_t>& HuffmanData::decodeRegionQ(size_t tablen, size_t bitsrem, std::vector<size_t>& accum) {
    if (bitsrem < 0) {
        if (bitsrem == -1) {
            return accum;
        }
        else throw "...";
    } 
    auto [coord, bitn] = decodeOneQuad(tablen);
    auto [a, b, c, d] = coord;
    accum.insert(accum.end(), {a,b,c,d});
    return decodeRegionQ(tablen, bitsrem - bitn, accum);
}

std::pair<Huffman::Tree<std::pair<size_t, size_t>>, size_t> huffmanDecodeTable(size_t) {

}

Huffman::Tree<std::tuple<size_t, size_t, size_t, size_t>> huffmanDecodeTableQuad(size_t) {

}

template<typename A>
template<typename Getter>
std::optional<std::pair<A, size_t>> Huffman::Tree<A>::lookup(Getter bit, size_t n) {
    if (state == State::EMPTY) return {};
    if (state == State::LEAF) return {{a, n}};
    if (bit.get()) return b.first->lookup(bit, n + 1);
    return b.second->lookup(bit, n + 1);
}

template<typename A>
std::optional<std::pair<A, size_t>> Huffman::Tree<A>::lookup(bool* bit, size_t n) {
    if (state == State::EMPTY) return {};
    if (state == State::LEAF) return {{a, n}};
    if (*bit) return b.first->lookup(bit, n + 1);
    return b.second->lookup(bit, n + 1);
}

template <typename A>
Huffman::Tree<A>::Tree() {
   state = State::EMPTY; 
}

template <typename A>
void Huffman::Tree<A>::insert(bool* v, size_t n, A val) {
    using namespace std;
    using namespace Huffman;
    if (state == State::EMPTY) {
        if (n == 0) {
            state = State::LEAF;
            a = val;
        }
        else {
            state = State::NODE;
            auto tree1 = new Tree<A>();
            auto tree2 = new Tree<A>();
            tree1->insert(v + 1, n - 1, val);
            if (*v) {
                b = make_pair(tree1, tree2);
            }
            else {
                b = make_pair(tree2, tree1);
            }
        }
    }
    else if (state == State::NODE) {
        if (*v) {
            b.first->insert(v + 1, n - 1, val);
        }
        else {
            b.second->insert(v + 1, n - 1, val);
        }
    }
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

double mp3FloatRep3(size_t a, size_t b) {
    if (a) {
       return pow(2.0, -1 * b); 
    }
    return pow(2.0, -0.5 * b);
}

BlockFlag toBlockFlag(bool mixedFlag, size_t blockType) {
    if (mixedFlag) return BlockFlag::MixedBlocks;
    if (blockType == 2) return BlockFlag::ShortBlocks;
    return BlockFlag::LongBlocks;
}

SideInfo::SideInfo(BitStream& bs, FrameHeader& header) {
    using namespace std;
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


/*
struct SideData {
    HuffmanData* sideHuffman;
    ScaleData* sideScalefactor;
    size_t sidePart23Length;
    size_t sideBlocktype;
    BlockFlag sideBlockflag;
    SideData(BitStream&, FrameHeader&);
    SideData();
    Scale parseRawScaleFactors(size_t, vector<size_t>);
};

struct ScaleData {
    double scaleGlobalGain;
    std::pair<size_t, size_t> slen;
    std::tuple<double, double, double> scaleSubblockGain;
    size_t scalefac;
    size_t scalePreflag;
};
*/

std::pair<std::vector<double>, std::vector<std::vector<double>>>
SideData::unpackScaleFactors(std::vector<size_t> large,
        std::vector<std::vector<size_t>> small) 
{
    std::vector<double> largeM (large.size());
    std::vector<std::vector<double>> smallM (small.size());
    if (sideScalefactor->scalePreflag) {
        for (size_t i{}; i < large.size(); ++i) {
            large[i] += tablePretab[i];
        }
    }
    for (size_t i{}; i < large.size(); ++i)  {
        largeM[i] = mp3FloatRep3(sideScalefactor->scalefac, large[i]);
    }
    for (size_t i{}; i < small.size(); ++i) {
        smallM[i].resize(small[i].size());
        for (size_t j{}; j < small[i].size(); ++j) {
            smallM[i][j] = mp3FloatRep3(sideScalefactor->scalefac, small[i][j]);
        }
    }
    return std::make_pair(largeM, smallM);
}

Scale SideData::parseRawScaleFactors(BitStream& bs, size_t scfsi, const std::vector<size_t>& gran0) {
    using namespace std;
    if (sideBlocktype == 2 && sideBlockflag == BlockFlag::MixedBlocks) {
        vector<size_t> scaleL0(22, 0);
        vector<vector<size_t>> scaleS(22, {0, 0, 0});
        generate(scaleL0.begin(), scaleL0.begin() + 8, [&]() {
            return bs.getBits(sideScalefactor->slen.first);
        });
        generate(scaleS.begin() + 3, scaleS.begin() + 6, [&]() {
            return vector<size_t> {
                bs.getBits(sideScalefactor->slen.first),
                bs.getBits(sideScalefactor->slen.first),
                bs.getBits(sideScalefactor->slen.first)
            };
        });
        generate(scaleS.begin() + 6, scaleS.end() + 13, [&]() {
            return vector<size_t> {
                bs.getBits(sideScalefactor->slen.second),
                bs.getBits(sideScalefactor->slen.second),
                bs.getBits(sideScalefactor->slen.second)
            };
        });
        auto bitsread = 8 * 9 * sideScalefactor->slen.first + 21 * sideScalefactor->slen.second;
        return {scaleL0, scaleS, bitsread};
    } 
    else if (sideBlocktype == 2) {
        auto bitsread = 8 * 9 * sideScalefactor->slen.first + 21 * sideScalefactor->slen.second;
        vector<vector<size_t>> scaleS(2, {0, 0, 0});
        generate(scaleS.begin(), scaleS.end() + 6, [&]() {
            return vector<size_t> {
                bs.getBits(sideScalefactor->slen.first),
                bs.getBits(sideScalefactor->slen.first),
                bs.getBits(sideScalefactor->slen.first)
            };
        });
        generate(scaleS.begin() + 6, scaleS.end() + 12, [&]() {
            return vector<size_t> {
                bs.getBits(sideScalefactor->slen.first),
                bs.getBits(sideScalefactor->slen.first),
                bs.getBits(sideScalefactor->slen.first)
            };
        });
        return {{}, scaleS, bitsread};
    }
    else {
        auto copyband0 = (!gran0.empty()) && (scfsi & 8);
        auto copyband1 = (!gran0.empty()) && (scfsi & 4);
        auto copyband2 = (!gran0.empty()) && (scfsi & 2);
        auto copyband3 = (!gran0.empty()) && (scfsi & 1);
        vector<size_t> scalefac(22, 0);

        if (copyband0) {
            copy(gran0.begin(), gran0.begin() + 6, scalefac.begin());
        }
        else {
            generate(scalefac.begin(), scalefac.begin() + 6, [&]() {
               return bs.getBits(sideScalefactor->slen.first); 
            }); 
        }

        if (copyband1) {
            copy(gran0.begin() + 6, gran0.begin() + 11, scalefac.begin() + 6);
        }
        else {
            generate(scalefac.begin() + 6, scalefac.begin() + 11, [&]() {
               return bs.getBits(sideScalefactor->slen.first); 
            }); 
        }

        if (copyband2) {
            copy(gran0.begin() + 11, gran0.begin() + 16, scalefac.begin() + 11);
        }
        else {
            generate(scalefac.begin() + 11, scalefac.begin() + 16, [&]() {
               return bs.getBits(sideScalefactor->slen.first); 
            }); 
        }

        if (copyband3) {
            copy(gran0.begin() + 16, gran0.begin() + 21, scalefac.begin() + 16);
        }
        else {
            generate(scalefac.begin() + 16, scalefac.begin() + 21, [&]() {
               return bs.getBits(sideScalefactor->slen.first); 
            }); 
        }
        
        auto bitsread = 6 * (copyband0 ? 0 : sideScalefactor->slen.first) +
                        5 * (copyband1 ? 0 : sideScalefactor->slen.first) +
                        5 * (copyband2 ? 0 : sideScalefactor->slen.first) +
                        5 * (copyband3 ? 0 : sideScalefactor->slen.first);
        return {scalefac, {{}}, bitsread};
    }
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
    HuffmanData huffdata {bs, bigvalues, make_tuple(r0len, r1len, r2len),
        make_tuple(table0, table1, table2), count1table};
    ScaleData scaledata {globalgain, {scalelengths[0], scalelengths[1]}, {subgain0, subgain1, subgain2},
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

bool BitStream::get() {
    return getBits(1);
}

size_t BitStream::getBits(size_t n) {
    if (!n) return 0;
    size_t res{};
    while (y < buf.size() && n) {
        res |= (buf[y] & (8 - i));
        inc(), n -= 1;
    } 
    if (n > 0) {
        size_t sz {(n >> 3) + 1};
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

size_t BitStream::lookAhead(size_t n) {
    if (!n) return 0;  
    size_t res{};
    if (buf.size() - y < (n >> 3) + 1) {
        std::vector<char> tmp((n >> 3) + 1 - buf.size() + y); 
        is >> tmp;
        buf.resize(buf.size() + tmp.size());
        std::copy(buf.begin() + y, buf.end(), tmp.begin());
    }
    auto _i{i}, _y{y};
    while (n) {
        res |= (buf[y] & (8 - i));
        inc(), n -= 1;
    }
    y = _y;
    i = _i;
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

FrameHeader::FrameHeader(uint32_t frame) : frame {frame} {
    using namespace std;
    static pair<bool, bool> mode[] = {{false, false}, 
        {true, false}, {false, true}, {true, true}};
    bitset<32> bs {frame};
    reverse(bs);
    cout << "FRAME BITS: ";
    for (int i = 0; i < 32; ++i) {
        cout << bs[i];
        if (!((i + 1) & 0b111)) cout << ' ';
    }
    reverse(bs);
    cout << endl;
    mpeg = fH::MPEG[bitSub(bs, 19, 20)];
    layer = fH::Layer[bitSub(bs, 17, 18)];
    crc = bs[16];
    bitrate = fH::bitrate[2][bitSub(bs, 12, 15) - 1];
    sampling_rate = fH::sampling_rate[0][bitSub(bs, 10, 11)];
    padding_bit = bs[9];
    channel_mode  = fH::channel_mode[bitSub(bs, 6, 7)];
    auto flb = (144 * 1000 * bitrate)/sampling_rate + padding_bit;
    auto modebits = bitSub(bs, 4, 5);
    is = mode[modebits].first;
    ms = mode[modebits].second;
}

size_t FrameHeader::length() {
    return (144 * 1000 * bitrate)/sampling_rate + padding_bit;
}


size_t Mp3::trackLength() {
    using namespace std;
    if (compType == Compression::CBR) {
        return (fsize - tagLen)/(lf->fh->bitrate*1000) * 8;
    }   
    else {
        return lf->fh->frameCount*1152/lf->fh->sampling_rate;
        return 0;
    }
}

Mp3::Mp3(const char* fn) : f{fn, std::ios::binary}, bs{f} {
    using namespace std;
    fsize = filesystem::file_size(fn);
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
    vector<char> frameBytes(4);
    f >> frameBytes;
    uint32_t frame = 0;
    for (size_t i{}; i < 4; ++i) {
        frame = frame | ((uint8_t)frameBytes[i] << ((3-i)*8));
    } 
    lf = new LogicalFrame();
    lf->fh = new FrameHeader(frame);
    DEBUG(marker);
    DEBUG(tagLen);
    DEBUG(lf->fh->frame);
    DEBUG(lf->fh->mpeg);
    DEBUG(lf->fh->layer);
    DEBUG(lf->fh->bitrate);
    DEBUG(lf->fh->sampling_rate);
    DEBUG(lf->fh->padding_bit);
    DEBUG(lf->fh->channel_mode);
    size_t xingOff = lf->fh->channel_mode != ChannelMode::Mono ? 32 : 17;
    DEBUG(xingOff);
    vector<char> sign(4);
    f.seekg(tagLen + ID3v2::finish + xingOff);
    f >> sign;
    lf->fh->frameCount = 0;
    if (sign == "Xing") {
        compType = Compression::XING;
        f.seekg(4, ios_base::cur);
        vector<char> scBytes(4);
        f >> scBytes;
        for (size_t i{}; i < 4; ++i) {
            lf->fh->frameCount = lf->fh->frameCount | ((uint8_t)scBytes[i] << ((3-i)*8));
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
                lf->fh->frameCount = lf->fh->frameCount | ((uint8_t)scBytes[i] << ((3-i)*8));
            } 
        }
        else compType = Compression::CBR;
    }
    DEBUG(lf->fh->frameCount);
    DEBUG((int)compType);
    f.seekg(tagLen + ID3v2::finish);
}

Mp3::~Mp3() {
    f.close();
}
