#ifndef play_scrub_h_
#define play_scrub_h_

#include <Arduino.h>
#include <AudioStream.h>
#include <SD.h>
#include "spi_interrupt.h"
class AudioPlayScrub : public AudioStream
{
public:
    AudioPlayScrub(void) : AudioStream(0, NULL), speed(-1.0), mode(0), playhead(0), targetPlayhead(0), isActive(false), scrubRateFactor(1.0) {}

    void setTarget(float target);                                 
    void setRate(float rate);                            
    bool setFile(const char *filename);
    void activate(bool active, int mode) { isActive = active; this->mode = mode; AudioStartUsingSPI();} 
    void setMode(int mode);
    virtual void update(void);
    void setSpeed(float speed);
    void grabBuffer(int16_t* buf);
    void setScrubBuffer(int16_t* buf);
    void stop(void);
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


    float speed;
      int mode;
    int bufCounter = 0;
    int index = 0;
    uint64_t fileSize;
    float oldPlayhead;
    float playhead;      
    float targetPlayhead;
    volatile bool isActive;    
    volatile float scrubRateFactor;
    File file;
    const uint bufLength = AUDIO_BLOCK_SAMPLES * 128;
    int16_t *buf;
    float playheadReference;
    float playheadReferenceEnd;
};

#endif // play_scrub_h_



