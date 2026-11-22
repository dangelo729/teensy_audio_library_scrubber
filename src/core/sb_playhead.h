// scrubby — core/sb_playhead.h
// the playhead: where a sound is heard, going after where it is wanted, as
// a record under a hand goes. nothing here knows what it runs on: no
// arduino, no audio library, no file.
//
// there is a target, the sample the sound is wanted at, and the playhead,
// the sample it is heard at, and every sample the playhead goes rate of the
// way to the target. so its speed, and with it the pitch, is how far behind
// it is: sought far, it races there and slows as it arrives; left alone, it
// comes to rest. the target may move too, speed samples a sample. at a rate
// of 1 the playhead is the target, and the sound plays at speed, back or
// on; at a lesser rate it spins up to a moving target as a turntable does,
// and spins down when the target stops. once the playhead is as fast as a
// target moving on at 1, within kLock, it is locked: put on a whole sample,
// it steps a whole sample at a time with its target, and what is heard is
// the sound's own samples, untouched.
//
// the sound may loop: its target goes round from its end to its start, or
// back, and the playhead with it, locked or not. or it may not: the target
// is held to its ends, and a locked playhead is let go at the end.
//
// between samples the sound is read through a cubic, four samples about
// the playhead, and it fades to nothing as the playhead slows under
// kQuiet, so a playhead at rest is silent.
//
// the sound need not all be in memory. the window is the part that is: a
// run of its samples from some sample on. the playhead is heard while the
// window holds what a read where it stands takes, the sample before it to
// the two after, and is silent while it does not. render can stop at the
// first sample the window does not hold, so whoever keeps the window can
// move it to where windowFor says and go on: starting with what the next
// read takes if the playhead is going on, ending with it if it is going
// back, so all of the window lies the way the playhead is going.
//
// the playhead and the target are doubles: a float cannot tell one sample
// from the next past a few minutes of sound.
#pragma once

#include <math.h>
#include <stdint.h>

namespace sb {

class Playhead {
public:
    // of 1, how near a target's speed of 1 the playhead's must be before it steps with it
    static constexpr double kLock = 0.002;
    // samples a sample, the playhead's speed under which the sound fades, to nothing at rest
    static constexpr double kQuiet = 0.02;
    // samples a sample, the fastest a target moves, either way
    static constexpr double kFastest = 1e6;

    // a sound count samples long, none of it in memory yet. the playhead
    // and the target go to its start, at rest
    void setSound(uint32_t count) {
        count_ = count;
        start_ = 0;
        have_ = 0;
        cut(0.0);
    }

    // the window: count samples of the sound, mono, from its sample start
    // on, the caller's to keep. what would lie past the sound's end is
    // left out of it
    void setWindow(const int16_t* samples, uint32_t start, uint32_t count) {
        samples_ = samples;
        start_ = start;
        have_ = !samples || start >= count_ ? 0 : count < count_ - start ? count : count_ - start;
    }

    // a sound whole in memory: count samples, mono, the caller's to keep
    void setBuffer(const int16_t* samples, uint32_t count) {
        setSound(count);
        setWindow(samples, 0, count);
    }

    // where a window length samples long starts to hold what the next
    // sample's read takes: at its start if the playhead is going on, at
    // its end if it is going back, in its middle if it is at rest. no
    // further than the sound's ends
    uint32_t windowFor(uint32_t length) const {
        if (length >= count_) return 0;
        Playhead next = *this;
        next.advance();
        const int64_t n = (int64_t)count_, room = n - (int64_t)length, at = next.index((int64_t)floor(next.playhead_));
        const int64_t first = at > 0 ? at - 1 : 0, last = at + 2 < n ? at + 2 : n - 1;
        const int64_t start = next.step_ > 0.0 ? first : next.step_ < 0.0 ? last + 1 - (int64_t)length : at - (int64_t)(length / 2);
        return (uint32_t)(start < 0 ? 0 : start > room ? room : start);
    }

    // of the way to its target the playhead goes each sample, over 0 and
    // no more than 1: less is a heavier record, slower to catch up
    void setRate(double rate) {
        rate_ = rate > 1e-9 ? (rate < 1.0 ? rate : 1.0) : 1e-9;
    }

    // samples a sample the target itself moves, back under 0: 1 plays, 0
    // stands. no faster than kFastest
    void setSpeed(double speed) {
        if (speed != speed) speed = 0.0;
        speed = speed > -kFastest ? (speed < kFastest ? speed : kFastest) : -kFastest;
        if (speed == speed_) return;
        speed_ = speed;
        locked_ = false;
    }

    // whether the sound loops. one that stops looping keeps its playhead
    // where it is heard, and its target, held to its ends
    void setLoop(bool on) {
        if (on == loop_) return;
        if (!on) playhead_ = place(playhead_);
        loop_ = on;
        locked_ = false;
        if (!on) {
            target_ = hold(target_);
            playhead_ = hold(playhead_);
        }
    }

    // the target put at sample at
    void setTarget(double at) {
        target_ = place(at);
        locked_ = false;
    }

    // the target and the playhead both put at sample at, at rest there
    void cut(double at) {
        target_ = playhead_ = place(at);
        step_ = 0.0;
        locked_ = false;
    }

    // the next n samples into out, -1 .. 1, a sample the window does not
    // hold silent. with stop, no further than the first of those, which is
    // left to be made again: how many were made
    int render(float* out, int n, bool stop = false) {
        for (int i = 0; i < n; i++) {
            const double target = target_, playhead = playhead_, step = step_;
            const bool locked = locked_;
            advance();
            const double whole = floor(playhead_);
            if (holds((int64_t)whole)) {
                const double fast = fabs(step_);
                out[i] = read((int64_t)whole, (float)(playhead_ - whole)) * (float)(fast < kQuiet ? fast / kQuiet : 1.0);
                continue;
            }
            if (stop) {
                target_ = target;
                playhead_ = playhead;
                step_ = step;
                locked_ = locked;
                return i;
            }
            out[i] = 0.0f;
        }
        return n;
    }

    double playhead() const { return place(playhead_); } // the sample it is heard at
    double target() const { return target_; }             // the sample it is wanted at
    double step() const { return step_; }                 // samples the playhead went over the last sample
    bool locked() const { return locked_; }               // it steps whole samples with its target
    uint32_t count() const { return count_; }             // the sound's length, samples

private:
    // the target and the playhead a sample on
    void advance() {
        const double n = (double)count_, last = count_ ? n - 1.0 : 0.0;
        if (locked_ && (loop_ || target_ + 1.0 <= last)) {
            target_ += 1.0;
            playhead_ += 1.0;
            if (target_ >= n) {
                target_ -= n;
                playhead_ -= n;
            }
            return;
        }
        locked_ = false;
        target_ += speed_;
        if (loop_ && count_) {
            // round the sound, the playhead as far, so it never has the
            // whole sound to go back over
            if (target_ < 0.0 || target_ >= n) {
                const double turns = floor(target_ / n) * n;
                target_ -= turns;
                playhead_ -= turns;
            }
            if (target_ >= n) {
                target_ -= n;
                playhead_ -= n;
            }
        } else target_ = hold(target_);
        step_ = (target_ - playhead_) * rate_;
        playhead_ += step_;
        if (speed_ == 1.0 && target_ < last && fabs(step_ - 1.0) < kLock) {
            // onto a whole sample, its target moved as far
            const double whole = floor(playhead_ + 0.5);
            target_ += whole - playhead_;
            playhead_ = whole;
            step_ = 1.0;
            locked_ = true;
        }
    }

    // at, no further than the sound's ends
    double hold(double at) const {
        const double last = count_ ? (double)(count_ - 1) : 0.0;
        return at > 0.0 ? (at < last ? at : last) : 0.0;
    }

    // at as a place in the sound: round it if it loops, else held to its ends
    double place(double at) const {
        if (!loop_ || !count_) return hold(at);
        if (at != at) return 0.0;
        const double n = (double)count_, in = at - floor(at / n) * n;
        return in < n ? in : 0.0;
    }

    // sample i as one of the sound's: round it if it loops, else its first
    // before its start and its last past its end
    int64_t index(int64_t i) const {
        const int64_t n = (int64_t)count_;
        if (!loop_) return i < 0 ? 0 : i >= n ? n - 1 : i;
        // a turn either way is the most there nearly ever is to take off
        if (i >= n) i -= n;
        else if (i < 0) i += n;
        if (i >= 0 && i < n) return i;
        const int64_t k = i % n;
        return k < 0 ? k + n : k;
    }

    // whether the window holds what a read at sample i takes: the samples
    // from the one before it to the two after, as far as the sound's ends
    bool holds(int64_t i) const {
        if (!have_) return false;
        const int64_t n = (int64_t)count_, at = index(i);
        const int64_t first = (at > 0 ? at - 1 : 0) - (int64_t)start_, last = (at + 2 < n ? at + 2 : n - 1) - (int64_t)start_;
        return first >= 0 && last < (int64_t)have_;
    }

    // the sample o on from sample i, whose read the window holds: the
    // sound's own, or, round a loop's seam where the window does not reach,
    // the nearest to it that the window holds
    float sample(int64_t i, int o) const {
        int64_t j = index(i + o) - (int64_t)start_;
        if (j < 0 || j >= (int64_t)have_) {
            j = index(i) - (int64_t)start_ + o;
            j = j < 0 ? 0 : j >= (int64_t)have_ ? (int64_t)have_ - 1 : j;
        }
        return (float)samples_[j];
    }

    // the sound t of the way from sample i to the next: a catmull-rom cubic
    // through the four samples about it, which passes through each sample
    // itself
    float read(int64_t i, float t) const {
        const float a = sample(i, -1), b = sample(i, 0), c = sample(i, 1), d = sample(i, 2);
        const float y = b + 0.5f * t * (c - a + t * (2.0f * a - 5.0f * b + 4.0f * c - d + t * (3.0f * (b - c) + d - a)));
        return y * (1.0f / 32768.0f);
    }

    const int16_t* samples_ = nullptr; // the window's samples
    uint32_t start_ = 0;               // the sample of the sound its first is
    uint32_t have_ = 0;                // how many it holds
    uint32_t count_ = 0;               // the sound's length, samples
    bool loop_ = false;
    double rate_ = 1.0;
    double speed_ = 0.0;
    double target_ = 0.0;
    double playhead_ = 0.0;
    double step_ = 0.0;
    bool locked_ = false;
};

}  // namespace sb
