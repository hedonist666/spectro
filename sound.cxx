#include "sound.h"
using namespace std;

int main() {
    Mp3 track("./materials/sound.mp3");
}

Mp3::Mp3(const char* fn) {
    f = fopen(fn, "r");
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
    uint32_t frame = 0;
    fread(frameBytes, 1, 4, f);
    for (size_t i{}; i < 4; ++i) {
        frame = frame | ((uint8_t)frameBytes[i] << ((i)*8));
    } 
    DEBUG(frame);
    for (int i = 0; i < 32; ++i) {
        cout << BIT(frame, i);
    }
    /*
    auto mpeg = 
    DEBUG(mpeg);
    if (strcmp(mpeg, "MPEG-1")) {
        cout << "not 1?" << endl;
    }
    auto layer = 
    DEBUG(layer);
    */
}

Mp3::~Mp3() {
    delete[] marker;
    fclose(f);
}
