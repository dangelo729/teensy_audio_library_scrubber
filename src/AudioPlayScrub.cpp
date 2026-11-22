// scrubby — AudioPlayScrub.cpp
// the header self-guards on #if defined(ARDUINO), so on a computer this
// translation unit is empty unless the host rig builds it against its
// stand-ins for the board.
#include "AudioPlayScrub.h"

#if defined(ARDUINO)

namespace {

// everything masked, for a short copy. PRIMASK is saved and restored
// rather than blindly re-enabled: a setter called from inside someone
// else's critical section must not end it. off the board there is nothing
// to mask
class IrqMask {
public:
    IrqMask() {
#if defined(__arm__)
        __asm__ volatile("mrs %0, primask\n" : "=r" (primask_) ::);
#endif
        __disable_irq();
    }
    ~IrqMask() { if (primask_ == 0) __enable_irq(); }
private:
    uint32_t primask_ = 0;
};

// x held to lo .. hi, and lo if it is no number
float within(float x, float lo, float hi) {
    return x > lo ? (x < hi ? x : hi) : lo;
}

// a sample in -1 .. 1 as 16 bits, held to their range
int16_t sixteen(float x) {
    const float v = x * 32768.0f;
    return v >= 32767.0f ? 32767 : v <= -32768.0f ? -32768 : (int16_t)lrintf(v);
}

}  // namespace

AudioPlayScrub::AudioPlayScrub(void) : AudioStream(0, NULL) {
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) scratch_[i] = 0.0f;
}

// ---- the setters: each lands whole at the top of the next block ------------------------------------

void AudioPlayScrub::activate(bool active, int mode) {
    IrqMask m;
    asked_.active = active;
    asked_.mode = mode;
}

void AudioPlayScrub::setMode(int mode) {
    IrqMask m;
    asked_.mode = mode;
}

void AudioPlayScrub::stop(void) {
    setMode(STOPPED);
}

void AudioPlayScrub::setTarget(float target) {
    IrqMask m;
    asked_.target = within(target, 0.0f, 1.0f);
}

void AudioPlayScrub::setRate(float rate) {
    IrqMask m;
    asked_.rate = within(rate, 0.0000001f, 1.0f);
}

void AudioPlayScrub::setSpeed(float speed) {
    IrqMask m;
    asked_.speed = speed != speed ? 0.0f : within(speed, -5.0f, 5.0f);
}

// ---- the window and the file -------------------------------------------------------------------------

void AudioPlayScrub::setScrubBuffer(int16_t* buf, uint32_t length) {
    IrqMask m;
    window_ = buf;
    length_ = buf ? length : 0;
    head_.setWindow(window_, 0, 0);     // it holds nothing yet: update() reads it in
}

bool AudioPlayScrub::setFile(const char* filename) {
    stop();
    busy_ = true;
    if (!guarded_) {
        AudioStartUsingSPI();
        guarded_ = true;
    }
    {
        IrqMask m;                      // as the audio library's sd players open a file
        file_ = SD.open(filename);
    }
    const uint64_t samples = file_ ? file_.size() / 2 : 0;
    count_ = samples > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)samples;
    head_.setSound(count_);
    if (count_ && window_ && length_) load();
    busy_ = false;
    return (bool)file_;
}

// the window read from the card about the playhead, and the playhead told
// what came: false if nothing did
bool AudioPlayScrub::load() {
    const uint32_t start = head_.windowFor(length_);
    const uint32_t want = count_ - start < length_ ? count_ - start : length_;
    size_t got = 0;
    if (file_.seek((uint64_t)start * 2)) got = file_.read(window_, (size_t)want * 2);
    if (got > (size_t)want * 2) got = 0;    // the card's error, read as a count
    head_.setWindow(window_, start, (uint32_t)(got / 2));
    return got >= 2;
}

// ---- the audio interrupt ---------------------------------------------------------------------------------

void AudioPlayScrub::update(void) {
    if (busy_) return;
    Asked a;
    {
        IrqMask m;
        a = asked_;
    }
    if (!a.active || (a.mode != PLAY && a.mode != SCRUB) || !window_ || !length_ || !count_) return;

    if (a.mode == PLAY) {
        if (sounding_ != PLAY) head_.cut(head_.playhead());    // on from where it is heard
        head_.setLoop(true);
        head_.setRate(1.0);
        head_.setSpeed(a.speed);
    } else {
        head_.setLoop(false);
        head_.setSpeed(0.0);
        head_.setRate(a.rate);
        head_.setTarget((double)a.target * (double)(count_ - 1));
    }
    sounding_ = a.mode;

    audio_block_t* block = allocate();
    if (!block) return;
    const int made = head_.render(scratch_, AUDIO_BLOCK_SAMPLES, true);
    if (made < AUDIO_BLOCK_SAMPLES) {
        // the playhead has left the window: the one read this block, and
        // whatever it still does not hold is silent
        if (!load()) {
            IrqMask m;
            asked_.mode = STOPPED;
        }
        head_.render(scratch_ + made, AUDIO_BLOCK_SAMPLES - made);
    }
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) block->data[i] = sixteen(scratch_[i]);
    transmit(block);
    release(block);
}

#endif  // defined(ARDUINO)
