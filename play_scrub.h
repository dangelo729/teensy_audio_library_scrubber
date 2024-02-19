#ifndef play_scrub_h_
#define play_scrub_h_

#include <Arduino.h>
#include <AudioStream.h>

class AudioPlayScrub : public AudioStream
{
public:
    AudioPlayScrub(void) : AudioStream(0, NULL), audioArray(nullptr), playhead(0), targetPlayhead(0), isActive(false), scrubRateFactor(1.0) {}

    void setTarget(float target);                                 // Set target playhead position (0.0 to 1.0)
    void setRate(float rate);                                     // Set scrub rate factor
    void setAudioArray(const int16_t *audioArray, size_t length); // Set the audio array for scrubbing
    void activate(bool active) { isActive = active; } // Activate the scrubber
    float calculatePlaybackRate(float distance);
    virtual void update(void);

private:
    template <typename T>
    int16_t lerp(int16_t a, int16_t b, float t)
    {
        return a + (b - a) * t;
    }
    float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
    {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }
    int clamp(int value, int min, int max) {
        return (value < min) ? min : (value > max) ? max : value;
    }

    const int16_t *audioArray; // Pointer to the audio samples array
    int16_t internalBuffer[AUDIO_BLOCK_SAMPLES];   // Internal buffer for audio samples
    int bufCounter = 0;
    int oldIndex = 0;
    int audioArrayLength;        // Length of the audio samples array
    float playhead;       // Current playhead index in the audio array
    float targetPlayhead; // Target playhead index in the audio array
    volatile bool isActive;         // Whether the scrubbing is active
    volatile float scrubRateFactor; // Rate at which the playhead approaches the target
};

#endif // play_scrub_h_