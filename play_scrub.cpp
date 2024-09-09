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
    targetPlayhead = mapFloat(target, 0.0, 1.0, 0, fileSize - 1);
}

bool AudioPlayScrub::setFile(const char *filename) 
{

    stop();
#if defined(HAS_KINETIS_SDHC)
    if (!(SIM_SCGC3 & SIM_SCGC3_SDHC))
        AudioStartUsingSPI();
#else
    AudioStartUsingSPI();
#endif
    __disable_irq();
    file = SD.open(filename);
    __enable_irq();
    if (!file)
    {
// Serial.println("unable to open file");
#if defined(HAS_KINETIS_SDHC)
        if (!(SIM_SCGC3 & SIM_SCGC3_SDHC))
            AudioStopUsingSPI();
#else
        AudioStopUsingSPI();
#endif
        return false;
    }
    fileSize = file.size();

    if (fileSize % 2 != 0)
    {
        fileSize--;
    }

    file.read(buf, bufLength * 2);
    playhead = 0;
    playheadReference = 0;
    targetPlayhead = 0;
    bufCounter = 0;
    // grabBuffer(buf);
    return true;
}

void AudioPlayScrub::stop(void)
{
    __disable_irq();
    if (mode != 3)
    {
        mode = 3;
       
      //  file.close();
         __enable_irq();
#if defined(HAS_KINETIS_SDHC)
        if (!(SIM_SCGC3 & SIM_SCGC3_SDHC))
            AudioStopUsingSPI();
#else
        AudioStopUsingSPI();
#endif
    }
    else
    {
        __enable_irq();
    }
}

void AudioPlayScrub::setRate(float rate)
{
    // Ensure rate is within a reasonable range
    // I've found that around 0.00001 - 0.0001 is a good range for nice sounding scrubbing, but play around
    scrubRateFactor = constrain(rate, 0.0000001f, 1.0f);
}

void AudioPlayScrub::setSpeed(float s)
{
    if (s > 5.0)
    {
        s = 5.0;
    }
    else if (s < -5.0)
    {
        s = -5.0;
    }

    this->speed = s;
}
void AudioPlayScrub::update(void)
{

    unsigned int i;
    // int16_t buf[bufLength];
    if (!isActive || mode == 3 || !buf)
    {
        return;
    }

    audio_block_t *block = allocate();
    if (block == nullptr)
    {
        return;
    }
    if (mode == 0) // playback mode
    {

        for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
        {

            // Adjust playhead based on speed
            playhead += speed * 2;

            index = round(mapFloat(playhead - playheadReference, 0, bufLength * 2, 0, bufLength));

            if (index < 0 || index >= bufLength || playhead >= fileSize || playhead < 0)
            {

                Serial.println("grab buffy");
                if (speed < 0)
                {
                    Serial.println("reverse");
                    // For reverse playback, ensure playheadReference is ahead of playhead
                    playheadReference -= bufLength * 2;
                    if (playheadReference < 0)
                    {
                        Serial.println("reverse wrap");
                        playheadReference = (fileSize-2) - bufLength * 2; // hmmmmmm
                        playhead = fileSize-2;
                    }
                }
                else
                {
                    // For forward playback, adjust playheadReference normally
                    playheadReference += bufLength * 2;
                    if (playheadReference > fileSize)
                    {
                        playheadReference = 0;
                        playhead = 0;
                    }
                }

                // Load new buffer
                file.seek(playheadReference, SeekSet);
                if (!file.read(buf, bufLength * 2))
                {

                    /*
                    Serial.print("index: ");
                    Serial.println(index);
                    Serial.print("playhead: ");
                    Serial.println(playhead);
                    Serial.print("playheadReference: ");
                    Serial.println(playheadReference);
                    Serial.print("Buflength: ");
                    Serial.println(bufLength);
                    Serial.print("filesize: ");
                    Serial.println(fileSize);
                    Serial.print("speed: ");
                    Serial.println(speed);
                    */
                    Serial.println("Buffer load failed");
                    return;
                }

                // After loading new buffer, reset index based on new playheadReference
                index = round(mapFloat(playhead - playheadReference, 0, bufLength * 2, 0, bufLength));
                // index = clamp(index, 0, bufLength - 1);
            }

            // Assign sample value to block data
            if (index >= 0 && index < bufLength)
            {
                block->data[i] = buf[index];
            }
            else
            {
                block->data[i] = 0; // Handle out-of-bounds index
            }
        }
    }
    else if (mode == 1) // Scrub mode
    {

        // index = round(playhead - playheadReference);

        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
        {
            float dif = targetPlayhead - playhead;
            // if (abs(dif) < 10)
            // {

            //     release(block);
            //     return;
            // }
            playhead += dif * scrubRateFactor;

            // index = round(playhead * .5 - playheadReference * .5);
            index = round(mapFloat(playhead - playheadReference, 0, bufLength * 2, 0, bufLength));

            // index = round(mapFloat(playhead, playheadReference, playheadReference+(bufLength*2), 0, bufLength));
            if (index < bufLength && index >= 0)
            {

                block->data[i] = buf[index];
                // bufCounter++;
            }
            else
            {

                grabBuffer(buf);

                index = round(mapFloat(playhead - playheadReference, 0, bufLength * 2, 0, bufLength));
                if (index >= 0 && index < bufLength)
                {
                    block->data[i] = buf[index];
                }
                else
                {
                    block->data[i] = 0;
                    Serial.println("no buffer");
                }
                // block->data[i] = buf[round(mapFloat(playhead, playheadReference, playheadReference+(bufLength*2), 0, bufLength))];
            }
        }
    }
    transmit(block);
    release(block);
}

void AudioPlayScrub::grabBuffer(int16_t *buf)
{
    if (file.available())
    {
        if (targetPlayhead < playhead)
        {

            playheadReference -= bufLength * 2;

            if (playheadReference < 0)
            {

                playheadReference = 0;
            }
        }
        else
        {

            playheadReference += bufLength * 2;

            if (playheadReference > fileSize - bufLength * 2)

            {

                playheadReference = fileSize - bufLength * 2;
            }
        }

        // playheadReferenceEnd = playheadReference + bufLength;

        file.seek(playheadReference, SeekSet);
        Serial.print("offset: ");
        Serial.println(file.read(buf, bufLength * 2));
    }
    else
    {

        file.seek(0); // this works apparently?
        return;
    }
}

void AudioPlayScrub::setScrubBuffer(int16_t *buf)
{
    this->buf = buf;
}

void AudioPlayScrub::setMode(int mode)
{
    if (mode == 3)
    {
        file.close();
    }
    this->mode = mode;
}


