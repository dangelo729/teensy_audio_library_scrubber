#include "play_scrub.h"
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <chrono>

void AudioPlayScrub::setTarget(float target)
{
    // Ensure target is within bounds and calculate target playhead index
    if (target > 1.0)
    {
        target = 1.0;
    }
    else if (target < 0.0)
    {
        target = 0.0;
    }
    targetPlayhead = target;
}

void AudioPlayScrub::setRate(float rate)
{
    // Ensure rate is within a reasonable range
    scrubRateFactor = constrain(rate, 0.00001f, 1.0f);
}

void AudioPlayScrub::setAudioArray(const int16_t *array, size_t length)
{
    audioArray = array;
    audioArrayLength = length;
    // Reset playhead positions
    playhead = 0;
    targetPlayhead = 0;
}


void AudioPlayScrub::update(void)
{
    
    // setFilterCutoff(10000);
    if (!isActive || audioArray == nullptr || audioArrayLength == 0)
        return;

    audio_block_t *block = allocate();
    if (block == nullptr)
        return;
 
    int index = round(mapFloat(playhead, 0.0, 1.0, 0, audioArrayLength));

    if (index >= 0 && index < audioArrayLength)
    {

        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
        {
         

            float dif = targetPlayhead - playhead;
            playhead += dif * scrubRateFactor;

            if (round(playhead*audioArrayLength) < audioArrayLength)
            {
                block->data[i] = audioArray[round(playhead*audioArrayLength)];
           
            }
            else
            {
                block->data[i] = 0;
            }
        }
        transmit(block, 0);
        release(block);
        bufCounter = 0;
        return;
    }
    else
    {
        Serial.println("release");
        Serial.println(playhead);
        release(block);
        return;
    }
}
