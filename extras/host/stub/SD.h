// scrubby — extras/host/stub/SD.h
// the sd card's stand-in on a computer. a file on it is a tape: a length
// and a rule that says each of its samples by its place, so a file of any
// length takes no room. the card counts what is read of it and can be
// made to fail.
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
    long reads = 0;     // how many reads have been asked of it
    int fail = 0;       // 0 it reads; 1 a read brings nothing; 2 a read answers with an error
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
        if (!tape_ || pos > tape_->bytes) return false;
        pos_ = pos;
        return true;
    }
    size_t read(void* buf, size_t nbyte) {
        ++host::card.reads;
        if (!tape_ || host::card.fail == 1) return 0;
        if (host::card.fail == 2) return (size_t)-1;
        const uint64_t left = tape_->bytes - pos_, n = nbyte < left ? nbyte : left;
        uint8_t* out = (uint8_t*)buf;
        for (uint64_t k = 0; k < n; ++k) {
            const uint64_t at = pos_ + k;
            const uint16_t s = (uint16_t)tape_->sample(at / 2);
            out[k] = (uint8_t)(at & 1 ? s >> 8 : s & 0xFF);
        }
        pos_ += n;
        return (size_t)n;
    }

private:
    const host::Tape* tape_ = nullptr;
    uint64_t pos_ = 0;
};

class SDClass {
public:
    File open(const char* name) {
        const auto it = host::card.files.find(name);
        return it == host::card.files.end() ? File() : File(&it->second);
    }
};
inline SDClass SD;
