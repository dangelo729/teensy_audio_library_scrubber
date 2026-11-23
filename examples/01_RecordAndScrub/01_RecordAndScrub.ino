// scrubby example 01: record and scrub
// record a sound to the sd card, then scrub through it with a pot, or play
// it at the pot's speed, backwards under the pot's middle. teensy 4.x and
// the audio shield (sgtl5000): its mic, its sd card, the scrubber on both
// channels.
//
//   record button   pin 17 to GND   press to record, press again to stop
//   play button     pin 16 to GND   starts the scrubber, and stops it
//   mode button     pin 15 to GND   scrub or play
//   pot             pin A11         where the sound is wanted, or how fast it plays
//
// the recording is the audio library's Recorder example's, which is in the
// public domain, and so is this.

#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Scrubby.h>

AudioInputI2S        i2sIn;
AudioRecordQueue     queue;
AudioPlayScrub       scrubber;
AudioOutputI2S       i2sOut;
AudioConnection      cordQueue(i2sIn, 0, queue, 0);
AudioConnection      cordLeft(scrubber, 0, i2sOut, 0), cordRight(scrubber, 0, i2sOut, 1);
AudioControlSGTL5000 codec;

Bounce buttonRecord = Bounce(17, 8);
Bounce buttonPlay = Bounce(16, 8);
Bounce buttonMode = Bounce(15, 8);

#define POT_PIN A11

// the audio shield's sd card. for the teensy 4.1's own: BUILTIN_SDCARD
#define SDCARD_CS_PIN 10

// which input on the audio shield is recorded: AUDIO_INPUT_MIC or AUDIO_INPUT_LINEIN
const int input = AUDIO_INPUT_MIC;

// the part of the file the scrubber keeps in memory
int16_t window[AudioPlayScrub::kWindow] DMAMEM;

File recording;                     // the file being recorded
bool isRecording = false;
bool sounding = false;              // the scrubber is started
int mode = AudioPlayScrub::SCRUB;
float smoothed = 0.0f;              // the pot's reading, smoothed
float steady = 0.0f;                // the smoothed reading as last heeded
elapsedMicros sincePot;             // since the pot was read

void startRecording() {
    // the sd library adds to the end of a file, so the last recording goes first
    if (SD.exists("RECORD.RAW")) SD.remove("RECORD.RAW");
    recording = SD.open("RECORD.RAW", FILE_WRITE);
    if (!recording) return;
    queue.begin();
    isRecording = true;
}

void continueRecording() {
    if (queue.available() < 2) return;
    // two blocks at a time: the sd library is quickest writing a whole 512 byte sector
    byte sector[512];
    memcpy(sector, queue.readBuffer(), 256);
    queue.freeBuffer();
    memcpy(sector + 256, queue.readBuffer(), 256);
    queue.freeBuffer();
    recording.write(sector, 512);
}

void stopRecording() {
    queue.end();
    while (queue.available() > 0) {
        recording.write((byte*)queue.readBuffer(), 256);
        queue.freeBuffer();
    }
    recording.close();
    isRecording = false;
    // the scrubber is given the recording, stopped at its start
    if (!scrubber.setFile("RECORD.RAW")) Serial.println("the recording will not open");
}

void setup() {
    pinMode(17, INPUT_PULLUP);
    pinMode(16, INPUT_PULLUP);
    pinMode(15, INPUT_PULLUP);

    // the record queue keeps what comes in here until it is written to the card
    AudioMemory(128);

    codec.enable();
    codec.inputSelect(input);
    codec.micGain(40);
    codec.volume(0.3f);

    scrubber.setScrubBuffer(window);
    scrubber.setRate(0.0001f);      // 0.00001 to 0.0001 scrubs well: less is a heavier record

    while (!SD.begin(SDCARD_CS_PIN)) {
        Serial.println("no sd card");
        delay(500);
    }
}

void loop() {
    buttonRecord.update();
    buttonPlay.update();
    buttonMode.update();

    // recording and the scrubber take turns with the card
    if (buttonRecord.fallingEdge()) {
        if (isRecording) stopRecording();
        else if (!sounding) startRecording();
    }
    if (isRecording) {
        continueRecording();
        return;
    }

    if (buttonMode.fallingEdge()) {
        mode = mode == AudioPlayScrub::SCRUB ? AudioPlayScrub::PLAY : AudioPlayScrub::SCRUB;
        if (sounding) scrubber.setMode(mode);
    }
    if (buttonPlay.fallingEdge()) {
        sounding = !sounding;
        if (sounding) scrubber.activate(true, mode);
        else scrubber.stop();
    }

    // left alone, the pot's reading still flickers a step either way: it is
    // smoothed over a tenth of a second, however often loop() runs, and
    // heeded only once it has moved more than a step
    smoothed += (analogRead(POT_PIN) - smoothed) * (1.0f - expf(-1e-6f * sincePot / 0.1f));
    sincePot = 0;
    if (fabsf(smoothed - steady) > 1.0f) steady = smoothed;
    const float pot = steady / 1023.0f;
    if (mode == AudioPlayScrub::SCRUB) scrubber.setTarget(pot);            // the pot is the sound, end to end
    else scrubber.setSpeed(2.0f * pot - 1.0f);                              // or its speed, -1 to 1
}
