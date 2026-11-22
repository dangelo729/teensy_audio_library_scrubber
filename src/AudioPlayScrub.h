// scrubby — AudioPlayScrub.h
// the one file that knows the platform. one AudioStream, no inputs, one
// output: a raw sound file on the sd card (16 bits a sample, mono, at the
// audio library's rate, as its recorder writes one), heard through
// sb::Playhead. the file is never in memory whole: the sketch lends a
// window, a run of samples in ram, and the window is read from the card
// about the playhead whenever the playhead leaves it.
//
// all of this header, includes too, sits inside #if defined(ARDUINO), so
// the umbrella Scrubby.h stays safe on a computer: there this header is
// nothing, and sb::Playhead is used directly.
//
// two modes, as the sketch picks:
//   PLAY    the sound plays at setSpeed, -5 .. 5 of its own, backwards
//           under 0, round and round the file.
//   SCRUB   the playhead goes after setTarget, 0 .. 1 of the way through
//           the file, setRate of the way there each sample: fast while it
//           is far, slowing as it arrives, silent at rest.
// a change of mode carries on from where the sound is heard.
//
// handoff. each setter writes what the sketch asks for inside a
// few-instruction interrupt mask (PRIMASK saved and restored), and
// update() copies it inside one, so a call lands at the top of the next
// block, whole. setFile changes what update() reads from: it raises a flag
// that makes update() return at once and lowers it when the new file is
// in. the audio interrupt can preempt the loop but never the reverse, so
// update() sees the old file, or nothing, or the new one. call the setters
// from the main loop.
//
// the card. update() reads it, as the audio library's own sd players do,
// and at most once a block: one window, when the playhead has left the one
// in memory. a playhead crossing the file faster than that keeps up with
// is heard in bursts. a read that brings nothing stops the scrubber, as
// stop() does. the sketch keeps off the card while the scrubber sounds:
// stop() it before writing a file. no Serial and no heap in here.
#pragma once

#if defined(ARDUINO)

#include <Arduino.h>
#include <AudioStream.h>
#include <SD.h>
#include <spi_interrupt.h>

#include "core/sb_playhead.h"

class AudioPlayScrub : public AudioStream {
public:
    enum Mode { PLAY = 0, SCRUB = 1, STOPPED = 3 };

    // samples, the window's length unless the sketch says another
    static constexpr uint32_t kWindow = AUDIO_BLOCK_SAMPLES * 128;

    // declare it globally, as every audio object, and never destroy it
    AudioPlayScrub(void);

    // the window: length samples of ram, the sketch's to keep (DMAMEM is a
    // good place for it). nothing sounds until it is lent
    void setScrubBuffer(int16_t* buf, uint32_t length = kWindow);

    // the file to play, from its start, the scrubber stopped: activate or
    // setMode starts it. false if it will not open, and then there is no
    // sound to play
    bool setFile(const char* filename);

    // sounding or not, and in which mode
    void activate(bool active, int mode);
    void setMode(int mode);     // PLAY, SCRUB or STOPPED
    void stop(void);            // setMode(STOPPED): it starts again where it stopped

    void setTarget(float target);   // SCRUB: where the sound is wanted, 0 its start .. 1 its end
    void setRate(float rate);       // SCRUB: of the way there the playhead goes each sample, 0.0000001 .. 1
    void setSpeed(float speed);     // PLAY: -5 .. 5 of the sound's own speed

    // the audio interrupt: what was asked for copied (masked), a block
    // made through sb::Playhead, the window read from the card once if the
    // playhead leaves it, held to 16 bits, transmitted, released
    virtual void update(void);

private:
    // what the sketch last asked for
    struct Asked {
        float target = 0.0f;
        float rate = 1.0f;
        float speed = 1.0f;
        int mode = PLAY;
        bool active = false;
    };

    bool load();

    Asked asked_;
    volatile bool busy_ = false;    // setFile is changing the file: update() returns at once
    bool guarded_ = false;          // the audio library has been told the audio interrupt uses the card
    File file_;
    uint32_t count_ = 0;            // the file's length, samples
    int16_t* window_ = nullptr;     // the window's ram,
    uint32_t length_ = 0;           // and how many samples it has room for
    int sounding_ = STOPPED;        // the mode the last block was made in
    sb::Playhead head_;
    float scratch_[AUDIO_BLOCK_SAMPLES];
};

#endif  // defined(ARDUINO)
