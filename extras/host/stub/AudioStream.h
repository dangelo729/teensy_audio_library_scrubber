// scrubby — extras/host/stub/AudioStream.h
// the audio library's stand-in on a computer: a pool of blocks that counts
// what is out of it, and the last block transmitted, for the tests to
// read.
#pragma once

#include "Arduino.h"

#define AUDIO_BLOCK_SAMPLES 128

struct audio_block_t {
    int16_t data[AUDIO_BLOCK_SAMPLES];
};

namespace host {

struct Audio {
    int pool = 8;           // how many blocks there are to hand out
    int out = 0;            // how many are out now
    long sent = 0;          // how many have been transmitted
    audio_block_t last{};   // the last of them
};
inline Audio audio;

}  // namespace host

class AudioStream {
public:
    AudioStream(unsigned char, audio_block_t**) {}
    virtual ~AudioStream() {}

protected:
    static audio_block_t* allocate(void) {
        if (host::audio.out >= host::audio.pool) return nullptr;
        ++host::audio.out;
        return new audio_block_t();
    }
    static void release(audio_block_t* block) {
        --host::audio.out;
        delete block;
    }
    void transmit(audio_block_t* block, unsigned char = 0) {
        host::audio.last = *block;
        ++host::audio.sent;
    }

private:
    virtual void update(void) = 0;
};
