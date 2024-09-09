// Example code for using the custom scrubber audio object
//This example is based on the recorder example from the teensy audio library
// Scrubber object and addition of scrubber to this example by: Peter D'Angelo
// Enjoy the scrubber let me know if you have issues or ideas
// Requires the audio shield:
//   http://www.pjrc.com/store/teensy3_audio.html
//
// Three pushbuttons need to be connected:
//   Record Button: pin 0 to GND
//   Stop Button:   pin 1 to GND
//   Play Button:   pin 2 to GND
//
// This example code is in the public domain.
// This example uses 3 buttons and a potentiometer to control the scrubber object

#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputI2S i2s2;      // xy=105,63
AudioAnalyzePeak peak1;  // xy=278,108
AudioRecordQueue queue1; // xy=281,63
AudioPlayScrub scrubber; // xy=302,157
AudioOutputI2S i2s1;     // xy=470,120
AudioConnection patchCord1(i2s2, 0, queue1, 0);
AudioConnection patchCord2(i2s2, 0, peak1, 0);
AudioConnection patchCord3(scrubber, 0, i2s1, 0);
AudioConnection patchCord4(scrubber, 0, i2s1, 1);
AudioControlSGTL5000 sgtl5000_1; // xy=265,212
// GUItool: end automatically generated code

// For a stereo recording version, see this forum thread:
// https://forum.pjrc.com/threads/46150?p=158388&viewfull=1#post158388

// A much more advanced sound recording and data logging project:
// https://github.com/WMXZ-EU/microSoundRecorder
// https://github.com/WMXZ-EU/microSoundRecorder/wiki/Hardware-setup
// https://forum.pjrc.com/threads/52175?p=185386&viewfull=1#post185386

// Bounce objects to easily and reliably read the buttons
Bounce buttonRecord = Bounce(17, 8);
Bounce buttonPlay = Bounce(16, 8);
Bounce buttonMode = Bounce(15, 8);

// which input on the audio shield will be used?
// const int myInput = AUDIO_INPUT_LINEIN;
const int myInput = AUDIO_INPUT_MIC;

// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7 // Teensy 4 ignores this, uses pin 11
#define SDCARD_SCK_PIN 14 // Teensy 4 ignores this, uses pin 13

// Potentiometer pin
#define POT_PIN A11

// Use these with the Teensy 3.5 & 3.6 & 4.1 SD card
// #define SDCARD_CS_PIN    BUILTIN_SDCARD
// #define SDCARD_MOSI_PIN  11  // not actually used
// #define SDCARD_SCK_PIN   13  // not actually used

// Use these for the SD+Wiz820 or other adaptors
// #define SDCARD_CS_PIN    4
// #define SDCARD_MOSI_PIN  11
// #define SDCARD_SCK_PIN   13

// Remember which mode we're doing
int mode = 0; // 0=stopped, 2=vari-speed playback, 3=scrubbing
int16_t scrubBuffer[AUDIO_BLOCK_SAMPLES * 128] DMAMEM;
// The file where data is recorded
File frec;
bool recording = false;
bool scrubberActive = false;

void startRecording()
{
  Serial.println("startRecording");
  if (SD.exists("RECORD.RAW"))
  {
    // The SD library writes new data to the end of the
    // file, so to start a new recording, the old file
    // must be deleted before new data is written.
    SD.remove("RECORD.RAW");
  }
  frec = SD.open("RECORD.RAW", FILE_WRITE);
  if (frec)
  {
    queue1.begin();
    mode = 1;
  }
}

void continueRecording()
{
  if (queue1.available() >= 2)
  {
    byte buffer[512];
    // Fetch 2 blocks from the audio library and copy
    // into a 512 byte buffer.  The Arduino SD library
    // is most efficient when full 512 byte sector size
    // writes are used.
    memcpy(buffer, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    memcpy(buffer + 256, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    // write all 512 bytes to the SD card
    // elapsedMicros usec = 0;
    frec.write(buffer, 512);
    // Uncomment these lines to see how long SD writes
    // are taking.  A pair of audio blocks arrives every
    // 5802 microseconds, so hopefully most of the writes
    // take well under 5802 us.  Some will take more, as
    // the SD library also must write to the FAT tables
    // and the SD card controller manages media erase and
    // wear leveling.  The queue1 object can buffer
    // approximately 301700 us of audio, to allow time
    // for occasional high SD card latency, as long as
    // the average write time is under 5802 us.
    // Serial.print("SD write, us=");
    // Serial.println(usec);
  }
}

void stopRecording()
{
  Serial.println("stopRecording");
  queue1.end();
  if (mode == 1)
  {
    while (queue1.available() > 0)
    {
      frec.write((byte *)queue1.readBuffer(), 256);
      queue1.freeBuffer();
    }
    frec.close();
  }

  if (scrubber.setFile("RECORD.RAW"))
  {
    Serial.println("scrubber file set");
  }
  else
  {
    Serial.println("scrubber file not set, this is bad");
  }
}

void adjustMicLevel()
{
  // TODO: read the peak1 object and adjust sgtl5000_1.micGain()
  // if anyone gets this working, please submit a github pull request :-)
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setup()
{
  // Configure the pushbutton pins
  pinMode(17, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);

  // Audio connections require memory, and the record queue
  // uses this memory to buffer incoming audio.
  AudioMemory(128);

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  sgtl5000_1.volume(0.3);
  scrubber.setRate(0.0001);
  scrubber.setScrubBuffer(scrubBuffer);
  sgtl5000_1.micGain(40);
  // Initialize the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN)))
  {
    // stop here if no SD card, but print a message
    while (1)
    {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
}

void loop()
{
  // First, read the buttons
  buttonRecord.update();
  buttonMode.update();
  buttonPlay.update();

  // Respond to button presses
  if (buttonRecord.fallingEdge())
  {
    Serial.println("Record Button Press");

    if (!recording && !scrubberActive)
    {
      startRecording();
      recording = true;
      return;
    }
    if (recording)
    {
      stopRecording();
      recording = false;
    }
  }


  if (buttonMode.fallingEdge())
  {
    Serial.println("Mode Button Press");
    if (!recording)
    {
      if (mode == 0)
      {
        Serial.println("scrub mode");
        mode = 1;
        scrubber.setMode(1);
      }
      else if (mode == 1)
      {
        Serial.println("playback mode");
        mode = 0;
        scrubber.setMode(0);
      }
    }
  }
  if (buttonPlay.fallingEdge())
  {
    Serial.println("Play Button Press");
    if (!recording)
    {

      if (scrubberActive)
      {
        scrubber.setSpeed(0.0);
        scrubber.stop();
        scrubberActive = false;
      }
      else
      {
        scrubber.activate(1, mode);//0 is playback, 1 is scrub
        scrubberActive = true;
      }
    }
  }

  // If we're playing or recording, carry on...
  if (recording)
  {
    continueRecording();
  }

  if (mode == 0)
  {

    scrubber.setSpeed(mapFloat(analogRead(POT_PIN), (float)0, (float)1023, -1.0, 1.0));
  }
  if (mode == 1)
  {
    scrubber.setTarget(mapFloat(analogRead(POT_PIN), (float)0, (float)1023, 0.0, 1.0));
  }
}
