// scrubby — extras/host/stub/SD.h
// the sd card's stand-in on a computer. a file on it is a tape: a length
// and a rule that says each of its samples by its place, so a file of any
// length takes no room. the card counts what is read of it and how often
// a file is sent back over itself, can be made to fail, and can call the
// test back while a file is being opened, as the audio interrupt might.
#pragma once

#include <map>
#include <string>

#include "Arduino.h"

namespace host {

struct Tape {
    uint64_t bytes = 0;                         // the file's length
    int16_t (*sample)(uint64_t i) = nullptr;    // its sample i
};

struct Card {
    std::map<std::string, Tape> files;
    long reads = 0;     // how many reads have been asked of it,
    long bytes = 0;     // how many bytes they brought,
    long back = 0;      // and how many seeks went back from where a file stood
    // 0 it reads; 1 a read brings nothing; 2 a read answers with an error;
    // 3 a seek fails; 4 a read brings half of what was asked
    int fail = 0;
    void (*opening)() = nullptr;    // called while a file is being opened
};
inline Card card;

}  // namespace host

class File {
public:
    File() {}
    explicit File(const host::Tape* tape) : tape_(tape) {}
    operator bool() const { return tape_ != nullptr; }
    uint64_t size() { return tape_ ? tape_->bytes : 0; }
    bool seek(uint64_t pos) {
        if (!tape_ || pos > tape_->bytes || host::card.fail == 3) return false;
        if (pos < pos_) ++host::card.back;
        pos_ = pos;
        return true;
    }
    size_t read(void* buf, size_t nbyte) {
        ++host::card.reads;
        if (!tape_ || host::card.fail == 1) return 0;
        if (host::card.fail == 2) return (size_t)-1;
        const uint64_t left = tape_->bytes - pos_, all = nbyte < left ? nbyte : left, n = host::card.fail == 4 ? all / 4 * 2 : all;
        uint8_t* out = (uint8_t*)buf;
        for (uint64_t k = 0; k < n; ++k) {
            const uint64_t at = pos_ + k;
            const uint16_t s = (uint16_t)tape_->sample(at / 2);
            out[k] = (uint8_t)(at & 1 ? s >> 8 : s & 0xFF);
        }
        pos_ += n;
        host::card.bytes += (long)n;
        return (size_t)n;
    }

private:
    const host::Tape* tape_ = nullptr;
    uint64_t pos_ = 0;
};

class SDClass {
public:
    File open(const char* name) {
        if (host::card.opening) host::card.opening();
        const auto it = host::card.files.find(name);
        return it == host::card.files.end() ? File() : File(&it->second);
    }
};
inline SDClass SD;
