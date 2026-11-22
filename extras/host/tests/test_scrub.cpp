// scrubby — tests/test_scrub.cpp
// AudioPlayScrub, the teensy object, built against the board's stand-ins
// (stub/): what it puts out block by block, what it reads of the card,
// and what it leaves out of the audio library's pool.
#include <cmath>
#include <cstdint>
#include <vector>

#include <Scrubby.h>

#include "sb_test.h"

namespace {

// a sample that says where it is: every place its own, near enough
int16_t wave(uint64_t i) {
    return (int16_t)((uint32_t)(i * 2654435761u) >> 16);
}

// a ramp of 20000 samples, a step a sample: a sample heard says how far
// along the ramp the playhead is
int16_t ramp(uint64_t i) {
    return (int16_t)((int64_t)(i % 20000) - 10000);
}

const int kBlock = AUDIO_BLOCK_SAMPLES;

// a scrubber with its window and a card, each test its own
struct Rig {
    std::vector<int16_t> window;    // just as long as it says, so a write past it is caught
    AudioPlayScrub s;

    explicit Rig(uint32_t length = AudioPlayScrub::kWindow) : window(length) {
        host::audio = host::Audio();
        host::card = host::Card();
        host::guards = 0;
        s.setScrubBuffer(window.data(), length);
    }

    // a file of count samples put on the card
    void file(const char* name, uint64_t count, int16_t (*rule)(uint64_t)) {
        host::card.files[name] = host::Tape{ count * 2, rule };
    }

    // the next block: whether one came. it is host::audio.last
    bool block() {
        const long sent = host::audio.sent;
        s.update();
        return host::audio.sent != sent;
    }

    const int16_t* out() const { return host::audio.last.data; }
};

}  // namespace

SB_TEST(scrub_is_silent_until_it_has_a_file_a_window_and_is_told_to_sound) {
    host::audio = host::Audio();
    host::card = host::Card();
    host::card.files["A.RAW"] = host::Tape{ 200000, wave };
    std::vector<int16_t> window(AudioPlayScrub::kWindow);
    AudioPlayScrub s;
    // nothing at all
    s.update();
    // told to sound with no file
    s.activate(true, AudioPlayScrub::SCRUB);
    s.setTarget(0.5f);
    for (int k = 0; k < 100; ++k) s.update();
    CHECK(host::audio.sent == 0 && host::card.reads == 0 && host::audio.out == 0);
    // a file and no window
    CHECK(s.setFile("A.RAW"));
    s.activate(true, AudioPlayScrub::PLAY);
    for (int k = 0; k < 100; ++k) s.update();
    CHECK(host::audio.sent == 0 && host::card.reads == 0);
    // the window lent: it sounds
    s.setScrubBuffer(window.data());
    s.update();
    CHECK(host::audio.sent == 1 && host::card.reads == 1);
    // a file that is not there: no sound to play
    CHECK(!s.setFile("NONE.RAW"));
    s.activate(true, AudioPlayScrub::PLAY);
    for (int k = 0; k < 100; ++k) s.update();
    CHECK(host::audio.sent == 1 && host::audio.out == 0);
}

SB_TEST(scrub_set_a_file_is_stopped_at_its_start) {
    Rig r;
    r.file("A.RAW", 100000, wave);
    r.file("B.RAW", 100000, ramp);
    CHECK(r.s.setFile("A.RAW"));
    CHECK(!r.block());
    r.s.activate(true, AudioPlayScrub::PLAY);
    for (int k = 0; k < 10; ++k) CHECK(r.block());
    // another file, while it sounds
    CHECK(r.s.setFile("B.RAW"));
    for (int k = 0; k < 10; ++k) CHECK(!r.block());
    r.s.setMode(AudioPlayScrub::PLAY);
    CHECK(r.block());
    for (int i = 0; i < kBlock; ++i) CHECK(r.out()[i] == ramp(1 + i));
}

SB_TEST(scrub_played_is_the_files_own_samples_round_and_round) {
    Rig r;
    const uint64_t count = 3 * 16384 + 1234;
    r.file("A.RAW", count, wave);
    r.s.setFile("A.RAW");
    r.s.setSpeed(1.0f);
    r.s.activate(true, AudioPlayScrub::PLAY);
    uint64_t at = 1;
    bool same = true;
    for (int k = 0; k < 1000; ++k) {
        CHECK(r.block());
        for (int i = 0; i < kBlock; ++i, ++at) same = same && r.out()[i] == wave(at % count);
    }
    CHECK(same);
    CHECK(at > 2 * count);
}

SB_TEST(scrub_played_backwards_is_the_file_backwards) {
    Rig r;
    const uint64_t count = 3 * 16384 + 1234;
    r.file("A.RAW", count, wave);
    r.s.setFile("A.RAW");
    r.s.setSpeed(-1.0f);
    r.s.activate(true, AudioPlayScrub::PLAY);
    uint64_t back = 1;
    bool same = true;
    for (int k = 0; k < 1000; ++k) {
        CHECK(r.block());
        for (int i = 0; i < kBlock; ++i, ++back) same = same && r.out()[i] == wave((count - back % count) % count);
    }
    CHECK(same);
}

SB_TEST(scrub_leaks_no_block_over_a_file_of_whole_windows) {
    Rig r;
    host::audio.pool = 2;
    const uint64_t count = 8 * 16384;   // 8 windows to the byte
    r.file("A.RAW", count, wave);
    r.s.setFile("A.RAW");
    r.s.setSpeed(1.0f);
    r.s.activate(true, AudioPlayScrub::PLAY);
    uint64_t at = 1;
    bool same = true, none = true;
    const int blocks = (int)(30 * count / kBlock);     // 30 times round
    for (int k = 0; k < blocks; ++k) {
        CHECK(r.block());
        none = none && host::audio.out == 0;
        for (int i = 0; i < kBlock; ++i, ++at) same = same && r.out()[i] == wave(at % count);
    }
    CHECK(none);
    CHECK(same);
    // and backwards
    r.s.setSpeed(-1.0f);
    for (int k = 0; k < blocks; ++k) {
        CHECK(r.block());
        none = none && host::audio.out == 0;
    }
    CHECK(none);
}

SB_TEST(scrub_reads_the_card_once_a_block_at_most) {
    Rig r;
    r.file("A.RAW", 120 * 44100, wave);     // two minutes
    r.s.setFile("A.RAW");
    r.s.activate(true, AudioPlayScrub::SCRUB);
    for (const float rate : { 0.0001f, 0.001f, 0.01f, 0.1f, 1.0f }) {
        r.s.setRate(rate);
        long most = 0, all = 0;
        for (const float target : { 1.0f, 0.0f, 0.37f }) {
            r.s.setTarget(target);
            for (int k = 0; k < 4000; ++k) {
                const long reads = host::card.reads;
                CHECK(r.block());
                const long did = host::card.reads - reads;
                most = did > most ? did : most;
                all += did;
            }
        }
        CHECK_MSG(most <= 1, "at rate %g a block read the card %ld times", rate, most);
        CHECK(all > 0);
        CHECK(host::audio.out == 0);
    }
    // played as fast as it goes, both ways
    r.s.setMode(AudioPlayScrub::PLAY);
    for (const float speed : { 5.0f, -5.0f }) {
        r.s.setSpeed(speed);
        for (int k = 0; k < 4000; ++k) {
            const long reads = host::card.reads;
            CHECK(r.block());
            CHECK(host::card.reads - reads <= 1);
        }
    }
}

SB_TEST(scrub_comes_to_rest_on_its_target_far_into_a_long_file) {
    Rig r;
    const uint64_t count = 10 * 60 * 44100 + 1;     // ten minutes, its middle a whole sample
    const uint64_t middle = (count - 1) / 2;
    r.file("A.RAW", count, wave);
    r.s.setFile("A.RAW");
    r.s.setRate(0.0001f);
    r.s.setTarget(0.5f);
    r.s.activate(true, AudioPlayScrub::SCRUB);
    for (int k = 0; k < 3500; ++k) CHECK(r.block());
    // at rest it is silent
    for (int i = 0; i < kBlock; ++i) CHECK(r.out()[i] == 0);
    // and played on from there, it is on the sample it was sent to
    r.s.setSpeed(1.0f);
    r.s.setMode(AudioPlayScrub::PLAY);
    CHECK(r.block());
    for (int i = 0; i < kBlock; ++i) CHECK_MSG(r.out()[i] == wave(middle + 1 + i), "sample %d", i);
}

SB_TEST(scrub_played_slow_far_into_a_long_file_still_moves) {
    Rig r;
    const uint64_t count = 8 * 60 * 44100 + 1;      // eight minutes
    r.file("A.RAW", count, ramp);
    r.s.setFile("A.RAW");
    // to its middle, four minutes in: sample 10584000, 4000 along the ramp
    r.s.setRate(0.01f);
    r.s.setTarget(0.5f);
    r.s.activate(true, AudioPlayScrub::SCRUB);
    for (int k = 0; k < 200; ++k) r.block();
    r.s.setMode(AudioPlayScrub::PLAY);
    for (const float speed : { 0.5f, 0.75f, 0.25f, -0.5f }) {
        r.s.setSpeed(speed);
        r.block();
        const int first = r.out()[0];
        for (int k = 0; k < 15; ++k) r.block();
        const int last = r.out()[kBlock - 1];
        const float went = (float)(last - first), want = speed * (16 * kBlock - 1);
        CHECK_MSG(std::fabs(went - want) <= 1.0f, "at %g it went %g samples, not %g", speed, went, want);
    }
}

SB_TEST(scrub_stopped_starts_again_where_it_stopped) {
    Rig r;
    r.file("A.RAW", 100000, wave);
    r.s.setFile("A.RAW");
    r.s.activate(true, AudioPlayScrub::PLAY);
    for (int k = 0; k < 10; ++k) CHECK(r.block());
    r.s.stop();
    for (int k = 0; k < 5; ++k) CHECK(!r.block());
    r.s.activate(true, AudioPlayScrub::PLAY);
    CHECK(r.block());
    for (int i = 0; i < kBlock; ++i) CHECK(r.out()[i] == wave(10 * kBlock + 1 + i));
    // not active, it is silent in any mode
    r.s.activate(false, AudioPlayScrub::PLAY);
    CHECK(!r.block());
    r.s.setMode(AudioPlayScrub::SCRUB);
    CHECK(!r.block());
    // and a mode that is none of its modes is silent
    r.s.activate(true, 2);
    CHECK(!r.block());
    r.s.setMode(-1);
    CHECK(!r.block());
    // the old numbers are its modes
    r.s.setMode(0);
    CHECK(r.block());
    r.s.setMode(3);
    CHECK(!r.block());
    r.s.setMode(1);
    CHECK(r.block());
    CHECK(host::audio.out == 0);
}

SB_TEST(scrub_changes_mode_from_where_it_is_heard) {
    Rig r;
    r.file("A.RAW", 100000, ramp);
    r.s.setFile("A.RAW");
    r.s.activate(true, AudioPlayScrub::PLAY);
    for (int k = 0; k < 10; ++k) CHECK(r.block());     // sample 1280
    // scrubbed from there half the way to its start a sample
    r.s.setRate(0.5f);
    r.s.setTarget(0.0f);
    r.s.setMode(AudioPlayScrub::SCRUB);
    CHECK(r.block());
    CHECK(r.out()[0] == ramp(640) && r.out()[1] == ramp(320) && r.out()[2] == ramp(160));
}

SB_TEST(scrub_holds_what_is_asked_to_its_range) {
    Rig r;
    const uint64_t count = 100000;
    r.file("A.RAW", count, ramp);
    r.s.setFile("A.RAW");
    r.s.activate(true, AudioPlayScrub::PLAY);
    // no faster than five times, either way
    r.s.setSpeed(100.0f);
    CHECK(r.block());
    for (int i = 0; i < kBlock; ++i) CHECK(r.out()[i] == ramp(5 * (i + 1)));
    r.s.setSpeed(-100.0f);
    CHECK(r.block());
    for (int i = 0; i < kBlock; ++i) CHECK(r.out()[i] == ramp(5 * kBlock - 5 * (i + 1)));
    // a target past the end is the end: played on from there, the file's start comes next
    r.s.setRate(1.0f);
    r.s.setTarget(7.0f);
    r.s.setMode(AudioPlayScrub::SCRUB);
    CHECK(r.block());
    r.s.setSpeed(1.0f);
    r.s.setMode(AudioPlayScrub::PLAY);
    CHECK(r.block());
    CHECK(r.out()[0] == ramp(0) && r.out()[1] == ramp(1));
    // no number at all is the start, and stands still
    r.s.setTarget(NAN);
    r.s.setRate(NAN);
    r.s.setMode(AudioPlayScrub::SCRUB);
    for (int k = 0; k < 20; ++k) CHECK(r.block());
    r.s.setRate(1.0f);
    CHECK(r.block());
    r.s.setSpeed(NAN);
    r.s.setMode(AudioPlayScrub::PLAY);
    CHECK(r.block());
    for (int i = 0; i < kBlock; ++i) CHECK(r.out()[i] == 0);
    r.s.setSpeed(1.0f);
    CHECK(r.block());
    CHECK(r.out()[0] == ramp(1));
    CHECK(host::audio.out == 0);
}

SB_TEST(scrub_stops_at_a_read_that_brings_nothing) {
    // a read that brings nothing, one that answers with an error, and a seek that fails before any read
    for (const int fail : { 1, 2, 3 }) {
        const long asked = fail == 3 ? 0 : 1;
        Rig r;
        host::audio.pool = 2;
        r.file("A.RAW", 1000000, wave);
        r.s.setFile("A.RAW");
        r.s.setSpeed(5.0f);
        r.s.activate(true, AudioPlayScrub::PLAY);
        for (int k = 0; k < 10; ++k) CHECK(r.block());
        host::card.fail = fail;
        // it plays on to the end of what it has, then asks the card once and stops
        const long reads = host::card.reads;
        int blocks = 0;
        while (r.block() && blocks < 1000) ++blocks;
        CHECK(blocks > 0 && blocks < 1000);
        CHECK(host::card.reads == reads + asked);
        for (int k = 0; k < 100; ++k) CHECK(!r.block());
        CHECK(host::card.reads == reads + asked);
        CHECK(host::audio.out == 0);
        // the card back, it is started again and sounds
        host::card.fail = 0;
        r.s.setSpeed(1.0f);
        r.s.activate(true, AudioPlayScrub::PLAY);
        CHECK(r.block());
        bool sound = false;
        for (int i = 0; i < kBlock; ++i) sound = sound || r.out()[i] != 0;
        CHECK(sound);
        CHECK(host::audio.out == 0);
    }
}

SB_TEST(scrub_never_writes_past_its_window) {
    Rig r(300);     // a window shorter than three blocks
    const uint64_t count = 500000;
    r.file("A.RAW", count, wave);
    r.s.setFile("A.RAW");
    r.s.setRate(0.001f);
    r.s.activate(true, AudioPlayScrub::SCRUB);
    for (const float target : { 1.0f, 0.0f, 0.5f }) {
        r.s.setTarget(target);
        for (int k = 0; k < 500; ++k) CHECK(r.block());
    }
    r.s.setMode(AudioPlayScrub::PLAY);
    for (const float speed : { 5.0f, -5.0f, 0.3f }) {
        r.s.setSpeed(speed);
        for (int k = 0; k < 500; ++k) CHECK(r.block());
    }
    // and through it, at its own speed, it is still the file's own samples
    r.s.setFile("A.RAW");
    r.s.setSpeed(1.0f);
    r.s.activate(true, AudioPlayScrub::PLAY);
    uint64_t at = 1;
    bool same = true;
    for (int k = 0; k < 200; ++k) {
        CHECK(r.block());
        for (int i = 0; i < kBlock; ++i, ++at) same = same && r.out()[i] == wave(at);
    }
    CHECK(same);
}

SB_TEST(scrub_plays_a_file_shorter_than_its_window) {
    Rig r;
    const uint64_t count = 1000;
    r.file("A.RAW", count, wave);
    r.s.setFile("A.RAW");
    r.s.setSpeed(1.0f);
    r.s.activate(true, AudioPlayScrub::PLAY);
    uint64_t at = 1;
    bool same = true;
    for (int k = 0; k < 30; ++k) {
        CHECK(r.block());
        for (int i = 0; i < kBlock; ++i, ++at) same = same && r.out()[i] == wave(at % count);
    }
    CHECK(same);
    r.s.setRate(0.01f);
    r.s.setMode(AudioPlayScrub::SCRUB);
    for (const float target : { 1.0f, 0.0f }) {
        r.s.setTarget(target);
        for (int k = 0; k < 50; ++k) CHECK(r.block());
    }
    // the whole of it came in with the file
    CHECK(host::card.reads == 1);
}

SB_TEST(scrub_is_silent_over_an_empty_file) {
    Rig r;
    r.file("NONE.RAW", 0, wave);
    host::card.files["ODD.RAW"] = host::Tape{ 1, wave };
    for (const char* name : { "NONE.RAW", "ODD.RAW" }) {
        CHECK(r.s.setFile(name));
        r.s.activate(true, AudioPlayScrub::PLAY);
        for (int k = 0; k < 10; ++k) CHECK(!r.block());
        r.s.setMode(AudioPlayScrub::SCRUB);
        r.s.setTarget(1.0f);
        for (int k = 0; k < 10; ++k) CHECK(!r.block());
    }
    CHECK(host::audio.out == 0);
}

SB_TEST(scrub_waits_when_there_is_no_block_to_be_had) {
    Rig r;
    r.file("A.RAW", 100000, wave);
    r.s.setFile("A.RAW");
    r.s.activate(true, AudioPlayScrub::PLAY);
    host::audio.pool = 0;
    for (int k = 0; k < 10; ++k) CHECK(!r.block());
    host::audio.pool = 1;
    CHECK(r.block());
    for (int i = 0; i < kBlock; ++i) CHECK(r.out()[i] == wave(1 + i));
}

SB_TEST(scrub_tells_the_audio_library_of_the_card_once) {
    Rig r;
    r.file("A.RAW", 100000, wave);
    CHECK(host::guards == 0);
    for (int k = 0; k < 3; ++k) {
        r.s.setFile("A.RAW");
        r.s.activate(true, AudioPlayScrub::PLAY);
        r.block();
        r.s.stop();
    }
    CHECK(host::guards == 1);
}

namespace {

// the scrubber the card calls back to while it opens a file, as the audio
// interrupt might come in the middle of setFile
AudioPlayScrub* opening = nullptr;
void interrupt() {
    opening->activate(true, AudioPlayScrub::PLAY);
    opening->update();
}

// a sample as update() holds it to 16 bits
int16_t sixteen(float x) {
    const float v = x * 32768.0f;
    return v >= 32767.0f ? 32767 : v <= -32768.0f ? -32768 : (int16_t)lrintf(v);
}

// full scale one way for two samples and the other for two: between the
// two alike a cubic swings past full scale
int16_t square(uint64_t i) {
    return i % 4 == 1 || i % 4 == 2 ? 32767 : -32768;
}

}  // namespace

SB_TEST(scrub_given_a_file_leaves_the_card_to_the_audio_interrupt) {
    Rig r;
    r.file("A.RAW", 100000, wave);
    r.file("B.RAW", 100000, ramp);
    // setFile opens the file and reads nothing of it
    r.s.setFile("A.RAW");
    CHECK(host::card.reads == 0);
    r.s.activate(true, AudioPlayScrub::PLAY);
    for (int k = 0; k < 3; ++k) CHECK(r.block());
    // the audio interrupt in the middle of setFile, told to sound: nothing is sent and nothing read
    opening = &r.s;
    host::card.opening = interrupt;
    const long sent = host::audio.sent, reads = host::card.reads;
    CHECK(r.s.setFile("B.RAW"));
    host::card.opening = nullptr;
    CHECK(host::audio.sent == sent && host::card.reads == reads && host::audio.out == 0);
    // and then it is the new file, from its start
    CHECK(r.block());
    for (int i = 0; i < kBlock; ++i) CHECK(r.out()[i] == ramp(1 + i));
}

SB_TEST(scrub_read_on_through_a_file_never_sends_the_card_back) {
    Rig r;
    r.file("A.RAW", 4000000, wave);
    r.s.setFile("A.RAW");
    r.s.setSpeed(1.0f);
    r.s.activate(true, AudioPlayScrub::PLAY);
    const int blocks = 2000;
    uint64_t at = 1;
    bool same = true;
    for (int k = 0; k < blocks; ++k) {
        CHECK(r.block());
        for (int i = 0; i < kBlock; ++i, ++at) same = same && r.out()[i] == wave(at);
    }
    CHECK(same);
    CHECK_MSG(host::card.back == 0, "%ld seeks went back", host::card.back);
    // each sample is read once: no more than the samples played and the window ahead of them
    const long most = 2L * (blocks * kBlock + (long)AudioPlayScrub::kWindow);
    CHECK_MSG(host::card.bytes <= most, "%ld bytes read for %d samples", host::card.bytes, blocks * kBlock);
    // faster and slower too
    for (const float speed : { 5.0f, 0.3f }) {
        r.s.setSpeed(speed);
        for (int k = 0; k < blocks; ++k) CHECK(r.block());
    }
    CHECK(host::card.back == 0);
}

SB_TEST(scrub_holds_its_sound_to_sixteen_bits) {
    Rig r;
    r.file("A.RAW", 100000, square);
    r.s.setFile("A.RAW");
    r.s.setSpeed(0.5f);
    r.s.activate(true, AudioPlayScrub::PLAY);
    CHECK(r.block());
    // half way between samples 1 and 2, both full scale up, and 3 and 4, both down
    CHECK(r.out()[2] == 32767);
    CHECK(r.out()[6] == -32768);
}

SB_TEST(scrub_changes_mode_in_the_middle_of_a_scrub_from_where_it_is_heard) {
    Rig r;
    r.file("A.RAW", 100000, ramp);
    r.s.setFile("A.RAW");
    r.s.setRate(0.0005f);
    r.s.setTarget(0.9f);
    r.s.activate(true, AudioPlayScrub::SCRUB);
    for (int k = 0; k < 3; ++k) CHECK(r.block());
    const int heard = r.out()[kBlock - 1];
    CHECK(heard > -10000 + 10000 && heard < -10000 + 20000);   // some way along the ramp, far from its target
    r.s.setSpeed(1.0f);
    r.s.setMode(AudioPlayScrub::PLAY);
    CHECK(r.block());
    CHECK_MSG(std::abs(r.out()[0] - (heard + 1)) <= 1, "heard %d, then %d", heard, r.out()[0]);
    for (int i = 1; i < kBlock; ++i) CHECK(r.out()[i] == r.out()[i - 1] + 1);
}

SB_TEST(scrub_lent_another_window_reads_it_in) {
    Rig r;
    r.file("A.RAW", 100000, wave);
    r.s.setFile("A.RAW");
    r.s.activate(true, AudioPlayScrub::PLAY);
    uint64_t at = 1;
    bool same = true;
    for (int k = 0; k < 5; ++k) {
        CHECK(r.block());
        for (int i = 0; i < kBlock; ++i, ++at) same = same && r.out()[i] == wave(at);
    }
    // the old one is the sketch's again, and written over
    std::vector<int16_t> other(2000, 0x5555);
    r.s.setScrubBuffer(other.data(), 2000);
    for (int16_t& x : r.window) x = 0x5555;
    for (int k = 0; k < 50; ++k) {
        CHECK(r.block());
        for (int i = 0; i < kBlock; ++i, ++at) same = same && r.out()[i] == wave(at);
    }
    CHECK(same);
    // a window too short for one read is none
    r.s.setScrubBuffer(other.data(), 3);
    CHECK(!r.block());
    r.s.setScrubBuffer(nullptr);
    CHECK(!r.block());
}

SB_TEST(scrub_plays_nothing_a_short_read_did_not_bring) {
    Rig r;
    r.file("A.RAW", 1000000, wave);
    for (int16_t& x : r.window) x = 0x5555;    // what is in the window before the card is read
    r.s.setFile("A.RAW");
    r.s.setSpeed(5.0f);
    r.s.activate(true, AudioPlayScrub::PLAY);
    host::card.fail = 4;
    uint64_t at = 5;
    long own = 0, other = 0;
    for (int k = 0; k < 400; ++k) {
        CHECK(r.block());
        for (int i = 0; i < kBlock; ++i, at += 5) {
            if (r.out()[i] == wave(at)) ++own;
            else if (r.out()[i] != 0) ++other;
        }
    }
    CHECK_MSG(other == 0, "%ld samples were neither the file's nor silent", other);
    CHECK(own > 0);
    CHECK(host::audio.out == 0);
}

SB_TEST(scrub_keeps_its_place_in_a_file_past_four_gigabytes) {
    Rig r;
    const uint64_t count = 3000000001ull;       // six gigabytes
    r.file("A.RAW", count, wave);
    r.s.setFile("A.RAW");
    r.s.setRate(0.01f);
    r.s.setTarget(0.9f);                        // past four gigabytes in
    r.s.activate(true, AudioPlayScrub::SCRUB);
    for (int k = 0; k < 60; ++k) CHECK(r.block());
    // played on from there: the sample after the one nearest where it was sent
    const uint64_t first = (uint64_t)std::floor((double)0.9f * (double)(count - 1) + 1.5);
    r.s.setSpeed(1.0f);
    r.s.setMode(AudioPlayScrub::PLAY);
    CHECK(r.block());
    for (int i = 0; i < kBlock; ++i) CHECK(r.out()[i] == wave(first + i));
}

SB_TEST(scrub_is_the_playhead_over_the_whole_file_or_silent) {
    Rig r;
    const uint32_t count = 400000;
    r.file("A.RAW", count, wave);
    std::vector<int16_t> all(count);
    for (uint32_t i = 0; i < count; ++i) all[i] = wave(i);
    sb::Playhead whole;
    whole.setBuffer(all.data(), count);
    r.s.setFile("A.RAW");
    r.s.activate(true, AudioPlayScrub::SCRUB);
    // each a setting, how long it is kept, and whether the card keeps up with it
    struct Move { int mode; float rate, speed, target; int blocks; bool keeps; };
    const Move moves[] = {
        { AudioPlayScrub::SCRUB, 0.01f, 0.0f, 0.5f, 100, false },     // to its middle, too fast to hear all of
        { AudioPlayScrub::PLAY, 0.0f, 0.5f, 0.0f, 100, true },
        { AudioPlayScrub::PLAY, 0.0f, -0.7f, 0.0f, 100, true },
        { AudioPlayScrub::PLAY, 0.0f, 3.3f, 0.0f, 50, true },
        { AudioPlayScrub::SCRUB, 0.0002f, 0.0f, 0.9f, 300, true },
        { AudioPlayScrub::SCRUB, 0.5f, 0.0f, 0.0f, 5, false },         // and across it in a few samples
        { AudioPlayScrub::SCRUB, 0.5f, 0.0f, 1.0f, 5, false },
        { AudioPlayScrub::PLAY, 0.0f, -1.0f, 0.0f, 50, true },
    };
    int was = AudioPlayScrub::STOPPED;
    float out[kBlock];
    for (const Move& m : moves) {
        long wrong = 0, silent = 0;
        r.s.setRate(m.rate);
        r.s.setSpeed(m.speed);
        r.s.setTarget(m.target);
        r.s.setMode(m.mode);
        for (int k = 0; k < m.blocks; ++k) {
            // the whole sound set as update() sets its playhead
            if (m.mode == AudioPlayScrub::PLAY) {
                if (was != AudioPlayScrub::PLAY) whole.cut(whole.playhead());
                whole.setLoop(true);
                whole.setRate(1.0);
                whole.setSpeed(m.speed);
            } else {
                whole.setLoop(false);
                whole.setSpeed(0.0);
                whole.setRate(m.rate);
                whole.setTarget((double)m.target * (double)(count - 1));
            }
            was = m.mode;
            whole.render(out, kBlock);
            CHECK(r.block());
            for (int i = 0; i < kBlock; ++i) {
                const int16_t want = sixteen(out[i]);
                if (r.out()[i] == want) continue;
                if (r.out()[i] == 0) ++silent;
                else ++wrong;
            }
        }
        CHECK_MSG(wrong == 0, "%ld samples were neither the sound's nor silent", wrong);
        if (m.keeps) CHECK_MSG(silent == 0, "%ld samples were silent", silent);
    }
    CHECK(host::audio.out == 0);
}
