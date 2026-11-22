// scrubby — tests/test_playhead.cpp
// the playhead on its own (src/core/sb_playhead.h): at rest, scrubbed,
// played, looped, spun up to a moving target, far into a long sound, and
// over a window that holds only part of the sound.
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

// a sample that is its place, for sounds short enough to count in 16 bits
int16_t place(uint64_t i) {
    return (int16_t)i;
}

std::vector<int16_t> sound(uint32_t count, int16_t (*rule)(uint64_t)) {
    std::vector<int16_t> s(count);
    for (uint32_t i = 0; i < count; ++i) s[i] = rule(i);
    return s;
}

// a sample as the playhead puts it out
float as(int16_t s) {
    return s * (1.0f / 32768.0f);
}

// a sound too long to keep, heard through a window that is moved to the
// playhead whenever render stops for it
struct Deck {
    sb::Playhead head;
    std::vector<int16_t> window;
    int16_t (*rule)(uint64_t);
    uint32_t count;
    long loads = 0;

    Deck(uint32_t count_, uint32_t length, int16_t (*rule_)(uint64_t)) : window(length), rule(rule_), count(count_) {
        head.setSound(count);
    }

    void load() {
        const uint32_t start = head.windowFor((uint32_t)window.size());
        const uint32_t have = count - start < window.size() ? count - start : (uint32_t)window.size();
        for (uint32_t k = 0; k < have; ++k) window[k] = rule(start + k);
        head.setWindow(window.data(), start, have);
        ++loads;
    }

    void render(float* out, int n) {
        int made = head.render(out, n, true);
        while (made < n) {
            load();
            const int more = head.render(out + made, n - made, true);
            made += more ? more : head.render(out + made, 1);
        }
    }
};

}  // namespace

SB_TEST(playhead_with_no_sound_is_silent) {
    sb::Playhead p;
    float out[64];
    p.setSpeed(1.0);
    CHECK(p.render(out, 64) == 64);
    for (float x : out) CHECK(x == 0.0f);
    p.setBuffer(nullptr, 0);
    p.setLoop(true);
    p.render(out, 64);
    for (float x : out) CHECK(x == 0.0f);
    CHECK(p.playhead() == 0.0);
}

SB_TEST(playhead_at_rest_is_silent) {
    const auto s = sound(1000, wave);
    sb::Playhead p;
    p.setBuffer(s.data(), 1000);
    float out[64];
    p.render(out, 64);
    for (float x : out) CHECK(x == 0.0f);
    CHECK(p.playhead() == 0.0 && p.step() == 0.0 && !p.locked());
}

SB_TEST(playhead_goes_rate_of_the_way_to_its_target_each_sample) {
    const auto s = sound(30000, place);
    sb::Playhead p;
    p.setBuffer(s.data(), 30000);
    p.setRate(0.01);
    p.setTarget(20000.0);
    float out[16];
    p.render(out, 1);
    CHECK(p.step() == 200.0 && p.playhead() == 200.0);
    CHECK(out[0] == as(200));
    p.render(out, 9);
    const double want = 20000.0 * (1.0 - std::pow(0.99, 10));
    CHECK_MSG(std::fabs(p.playhead() - want) < 1e-6, "at %.9f, wanted %.9f", p.playhead(), want);
}

SB_TEST(playhead_comes_to_rest_on_its_target_in_silence) {
    const auto s = sound(30000, wave);
    sb::Playhead p;
    p.setBuffer(s.data(), 30000);
    p.setRate(0.001);
    p.setTarget(20000.0);
    std::vector<float> out(40000);
    p.render(out.data(), 40000);
    CHECK_MSG(std::fabs(p.playhead() - 20000.0) < 1e-6, "at %.9f", p.playhead());
    CHECK(p.target() == 20000.0);
    for (int i = 40000 - 64; i < 40000; ++i) CHECK(std::fabs(out[i]) < 1e-6f);
}

SB_TEST(playhead_goes_back_as_it_goes_on) {
    const auto s = sound(30000, place);
    sb::Playhead p;
    p.setBuffer(s.data(), 30000);
    p.cut(20000.0);
    p.setRate(0.01);
    p.setTarget(1000.0);
    float out[4];
    p.render(out, 1);
    CHECK(p.step() == -190.0 && p.playhead() == 19810.0);
    CHECK(out[0] == as(19810));
    std::vector<float> rest(5000);
    p.render(rest.data(), 5000);
    CHECK(std::fabs(p.playhead() - 1000.0) < 1e-6);
}

SB_TEST(playhead_played_is_the_sounds_own_samples) {
    const auto s = sound(5000, wave);
    sb::Playhead p;
    p.setBuffer(s.data(), 5000);
    p.setSpeed(1.0);
    float out[256];
    p.render(out, 256);
    for (int i = 0; i < 256; ++i) CHECK(out[i] == as(s[i + 1]));
    CHECK(p.locked() && p.step() == 1.0 && p.playhead() == 256.0);
}

SB_TEST(playhead_with_no_loop_runs_out_at_the_end_and_rests) {
    const auto s = sound(1000, wave);
    sb::Playhead p;
    p.setBuffer(s.data(), 1000);
    p.setSpeed(1.0);
    std::vector<float> out(1200);
    p.render(out.data(), 1200);
    for (int i = 0; i < 999; ++i) CHECK(out[i] == as(s[i + 1]));
    for (int i = 999; i < 1200; ++i) CHECK(out[i] == 0.0f);
    CHECK(p.playhead() == 999.0 && p.target() == 999.0 && !p.locked());
}

SB_TEST(playhead_looping_goes_round_and_round) {
    const auto s = sound(1000, wave);
    sb::Playhead p;
    p.setBuffer(s.data(), 1000);
    p.setLoop(true);
    p.setSpeed(1.0);
    std::vector<float> out(3500);
    p.render(out.data(), 3500);
    for (int i = 0; i < 3500; ++i) CHECK(out[i] == as(s[(i + 1) % 1000]));
    CHECK(p.playhead() == 500.0);
}

SB_TEST(playhead_looping_goes_round_backwards) {
    const auto s = sound(1000, wave);
    sb::Playhead p;
    p.setBuffer(s.data(), 1000);
    p.setLoop(true);
    p.setSpeed(-1.0);
    std::vector<float> out(2500);
    p.render(out.data(), 2500);
    for (int i = 0; i < 2500; ++i) CHECK(out[i] == as(s[(999 - i % 1000) % 1000]));
}

SB_TEST(playhead_reads_between_the_samples_and_over_them) {
    const auto s = sound(30000, place);
    sb::Playhead p;
    float out[200];
    // at half speed, half way between each two: a cubic through a straight line is the line
    p.setBuffer(s.data(), 30000);
    p.cut(100.0);
    p.setSpeed(0.5);
    p.render(out, 200);
    for (int i = 0; i < 200; ++i) CHECK_MSG(std::fabs(out[i] * 32768.0f - (100.0f + 0.5f * (i + 1))) < 1e-3f, "sample %d is %f", i, out[i] * 32768.0f);
    // at five times, every fifth
    p.setBuffer(s.data(), 30000);
    p.setSpeed(5.0);
    p.render(out, 200);
    for (int i = 0; i < 200; ++i) CHECK(out[i] == as((int16_t)(5 * (i + 1))));
}

SB_TEST(playhead_fades_as_it_slows) {
    const auto s = sound(30000, place);
    sb::Playhead p;
    p.setBuffer(s.data(), 30000);
    p.cut(10000.0);
    p.setSpeed(sb::Playhead::kQuiet / 2.0);
    float out[1];
    p.render(out, 1);
    // half as loud as the sound is there
    CHECK_MSG(std::fabs(out[0] * 32768.0f - 5000.0f) < 1.0f, "%f", out[0] * 32768.0f);
}

SB_TEST(playhead_spins_up_to_a_moving_target_and_locks_on) {
    const uint32_t count = 20 * 44100;
    const int lag = 13230;     // 0.3 s
    const auto s = sound(count, wave);
    sb::Playhead p;
    p.setBuffer(s.data(), count);
    p.setRate(1.0 - std::exp(-1.0 / lag));
    p.setSpeed(1.0);
    std::vector<float> out(lag);
    p.render(out.data(), lag);
    CHECK_MSG(std::fabs(p.step() - (1.0 - 1.0 / M_E)) < 0.01, "going %.4f", p.step());
    CHECK(!p.locked());
    int waited = 0;
    while (!p.locked() && waited++ < 5 * 44100) p.render(out.data(), 1);
    CHECK(p.locked() && p.step() == 1.0);
    CHECK(p.playhead() == std::floor(p.playhead()));
    CHECK_MSG(std::fabs(p.target() - p.playhead() - lag) < 0.01 * lag, "%.2f behind", p.target() - p.playhead());
    const int64_t from = (int64_t)p.playhead();
    p.render(out.data(), 1000);
    for (int i = 0; i < 1000; ++i) CHECK(out[i] == as(s[from + 1 + i]));
    // the target stopped, it spins down and rests on it
    p.setSpeed(0.0);
    CHECK(!p.locked());
    for (int k = 0; k < 40; ++k) p.render(out.data(), lag);
    CHECK(std::fabs(p.target() - p.playhead()) < 1e-6);
    CHECK(std::fabs(out[lag - 1]) < 1e-6f);
}

SB_TEST(playhead_spun_up_goes_round_a_loop_on_the_sounds_own_samples) {
    const uint32_t count = 5000;
    const auto s = sound(count, wave);
    sb::Playhead p;
    p.setBuffer(s.data(), count);
    p.setLoop(true);
    p.setRate(0.01);
    p.setSpeed(1.0);
    float one[1];
    int waited = 0;
    while (!p.locked() && waited++ < 100000) p.render(one, 1);
    CHECK(p.locked());
    const int64_t from = (int64_t)p.playhead();
    std::vector<float> out(12000);
    p.render(out.data(), 12000);
    for (int i = 0; i < 12000; ++i) CHECK(out[i] == as(s[(from + 1 + i) % count]));
    CHECK(p.locked());
}

SB_TEST(playhead_keeps_its_place_far_into_a_long_sound) {
    const uint32_t count = 30u * 60u * 44100u;     // half an hour
    Deck deck(count, 16384, wave);
    std::vector<float> out(4096);
    // scrubbed to its end, it rests on it
    deck.head.setRate(0.0001);
    deck.head.setTarget(count - 1.0);
    for (int k = 0; k < 100; ++k) deck.render(out.data(), 4096);
    CHECK_MSG(std::fabs(deck.head.target() - deck.head.playhead()) < 0.001, "%.6f short", deck.head.target() - deck.head.playhead());
    // played at a quarter speed twenty minutes in, it goes a quarter as far
    deck.head.cut(52920000.0);
    deck.head.setRate(1.0);
    deck.head.setSpeed(0.25);
    deck.render(out.data(), 4000);
    CHECK_MSG(deck.head.playhead() == 52921000.0, "at %.3f", deck.head.playhead());
    // and at its own speed it is the sound's own samples there
    deck.head.setSpeed(1.0);
    deck.render(out.data(), 4096);
    for (int i = 0; i < 4096; ++i) CHECK(out[i] == as(wave(52921001u + i)));
}

SB_TEST(playhead_stops_at_the_first_sample_its_window_does_not_hold) {
    const auto s = sound(100000, wave);
    sb::Playhead p;
    p.setSound(100000);
    p.setWindow(s.data(), 0, 1000);
    p.setSpeed(1.0);
    std::vector<float> out(2000);
    CHECK(p.render(out.data(), 2000, true) == 999);
    for (int i = 0; i < 999; ++i) CHECK(out[i] == as(s[i + 1]));
    CHECK(p.playhead() == 999.0 && p.locked());
    // asked again with the same window, it has made nothing more
    CHECK(p.render(out.data(), 2000, true) == 0);
    CHECK(p.playhead() == 999.0);
    // the window moved to where it goes next, it goes on with that sample
    CHECK(p.windowFor(1000) == 500);
    p.setWindow(s.data() + 500, 500, 1000);
    CHECK(p.render(out.data(), 400, true) == 400);
    for (int i = 0; i < 400; ++i) CHECK(out[i] == as(s[1000 + i]));
}

SB_TEST(playhead_says_where_a_window_should_start) {
    sb::Playhead p;
    p.setSound(100000);
    CHECK(p.windowFor(1000) == 0);
    p.cut(50000.0);
    CHECK(p.windowFor(1000) == 49500);
    CHECK(p.windowFor(1001) == 49500);
    p.cut(99999.0);
    CHECK(p.windowFor(1000) == 99000);
    CHECK(p.windowFor(100000) == 0);
    CHECK(p.windowFor(200000) == 0);
}

SB_TEST(playhead_is_silent_where_its_window_holds_nothing_and_goes_on) {
    const auto s = sound(100000, wave);
    sb::Playhead p;
    p.setSound(100000);
    p.setWindow(s.data(), 0, 1000);
    p.cut(5000.0);
    p.setSpeed(1.0);
    float out[64];
    CHECK(p.render(out, 64) == 64);
    for (float x : out) CHECK(x == 0.0f);
    CHECK(p.playhead() == 5064.0);
}

SB_TEST(playhead_at_its_windows_edge_reads_the_nearest_sample_it_holds) {
    // the window is the sound's samples 100 .. 199 and nothing else, on the heap, so a read past it is caught
    const std::vector<int16_t> window = [] {
        std::vector<int16_t> w(100);
        for (int k = 0; k < 100; ++k) w[k] = (int16_t)(100 + k);
        return w;
    }();
    sb::Playhead p;
    float out[1];
    // half way past its last sample
    p.setSound(1000);
    p.setWindow(window.data(), 100, 100);
    p.cut(199.0);
    p.setSpeed(0.5);
    CHECK(p.render(out, 1, true) == 1);
    CHECK_MSG(out[0] * 32768.0f >= 199.0f && out[0] * 32768.0f <= 199.5f, "%f", out[0] * 32768.0f);
    // half way from its first sample to its second, going back
    p.cut(101.0);
    p.setSpeed(-0.5);
    CHECK(p.render(out, 1, true) == 1);
    CHECK_MSG(out[0] * 32768.0f >= 100.0f && out[0] * 32768.0f <= 101.0f, "%f", out[0] * 32768.0f);
    // looping, at the sound's seam, with only its end in the window
    p.setSound(200);
    p.setWindow(window.data(), 100, 100);
    p.setLoop(true);
    p.cut(199.0);
    p.setSpeed(0.5);
    CHECK(p.render(out, 1, true) == 1);
    CHECK_MSG(out[0] * 32768.0f >= 199.0f && out[0] * 32768.0f <= 199.5f, "%f", out[0] * 32768.0f);
}

SB_TEST(playhead_holds_wild_values) {
    const auto s = sound(1000, wave);
    sb::Playhead p;
    p.setBuffer(s.data(), 1000);
    std::vector<float> out(4096);
    p.setTarget(NAN);
    CHECK(p.target() == 0.0);
    p.setTarget(1e30);
    CHECK(p.target() == 999.0);
    p.setTarget(-1e30);
    CHECK(p.target() == 0.0);
    p.cut(-5.0);
    CHECK(p.playhead() == 0.0);
    p.setRate(0.0);
    p.setRate(NAN);
    p.setSpeed(NAN);
    p.render(out.data(), 4096);
    CHECK(p.playhead() == 0.0);
    for (const bool loop : { false, true }) {
        p.setLoop(loop);
        p.setRate(1.0);
        p.setSpeed(INFINITY);
        p.render(out.data(), 4096);
        p.setSpeed(-INFINITY);
        p.render(out.data(), 4096);
        p.setRate(1e-9);
        p.render(out.data(), 4096);
        CHECK(p.playhead() >= 0.0 && p.playhead() < 1000.0);
        for (float x : out) CHECK(x == x && std::fabs(x) <= 2.0f);
    }
}
