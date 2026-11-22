# Scrubby

Scrubby is a scrubber for the Teensy Audio Library: it plays a sound file from the SD card the way
a record plays under a hand. Tell it where in the file you want the sound and it goes there, fast
while it is far and slowing as it arrives, its pitch following its speed. Or let it play, at any
speed from five times forwards to five times backwards.

Feel free to throw it into your projects. If you use it, please tell me!

## Install

- Arduino IDE: download this repository as a ZIP, then Sketch > Include Library > Add .ZIP Library.
- PlatformIO: `lib_deps = https://github.com/dangelo729/teensy_audio_library_scrubber.git`

It needs a Teensy 4.x and the Teensy Audio Library (in Teensyduino); then `#include <Scrubby.h>`.

## Use

```cpp
#include <Audio.h>
#include <SD.h>
#include <Scrubby.h>

AudioPlayScrub       scrubber;
AudioOutputI2S       i2s;
AudioConnection      cordLeft(scrubber, 0, i2s, 0), cordRight(scrubber, 0, i2s, 1);
AudioControlSGTL5000 codec;

int16_t window[AudioPlayScrub::kWindow] DMAMEM;     // the part of the file kept in memory

void setup() {
    AudioMemory(8);
    codec.enable();
    codec.volume(0.5f);
    SD.begin(BUILTIN_SDCARD);
    scrubber.setScrubBuffer(window);
    scrubber.setFile("SOUND.RAW");
    scrubber.setRate(0.0001f);                      // how quickly it catches up
    scrubber.activate(true, AudioPlayScrub::SCRUB);
}

void loop() {
    scrubber.setTarget(analogRead(A0) / 1023.0f);   // a pot: 0 is the file's start, 1 its end
}
```

`AudioPlayScrub` is one audio object with no inputs and one output:

- `setScrubBuffer(buffer)` lends it a window of RAM, `AudioPlayScrub::kWindow` samples long
  (16,384: a third of a second, 32 KB). The file is never in memory whole; this is the part of it
  the playhead is in. `setScrubBuffer(buffer, length)` lends a window of another length.
- `setFile(name)` chooses the file and leaves the scrubber stopped at its start. It returns false
  if the file will not open.
- `activate(true, mode)` starts it in a mode, `setMode(mode)` changes mode, and `stop()` stops it
  where it is. The modes are `AudioPlayScrub::SCRUB`, `PLAY` and `STOPPED`. A change of mode carries
  on from wherever the sound is.
- In `SCRUB`, `setTarget(place)` says where the sound is wanted, from 0 at the file's start to 1 at
  its end, and `setRate(rate)` says how much of the way there it goes each sample. 0.00001 to
  0.0001 scrubs well: less is a heavier record, slower to catch up. At rest it is silent.
- In `PLAY`, `setSpeed(speed)` plays the file at -5 to 5 times its own speed, backwards under 0,
  round and round.

The file is raw sound: 16 bits a sample, mono, 44.1 kHz, no header, as the Audio Library's Recorder
example writes one. Every call lands at the start of the next audio block (128 samples, 2.9 ms).
Declare the object globally, as every Teensy Audio object.

The scrubber reads the card from the audio interrupt, as the Audio Library's own SD players do, so
`stop()` it before your sketch writes to the card, and start it again after. If the sketch wrote
over the file it plays, give it the file again with `setFile`.

## Examples

- `01_RecordAndScrub`: record a sound with the audio shield's mic, then scrub through it with a pot
  or play it at the pot's speed.

## On a computer

The scrubber itself (`src/core/sb_playhead.h`) is plain C++ with no Arduino in it, and plays a
sound from memory anywhere:

```cpp
#include <Scrubby.h>

sb::Playhead head;
head.setBuffer(samples, count);     // a sound whole in memory: 16-bit, mono
head.setRate(0.0001);
head.setTarget(44100.0);            // the sample it is wanted at
head.render(out, 128);              // the next 128 samples, -1 to 1
```

`make test` runs the tests with clang: the playhead's, and `AudioPlayScrub`'s against a stand-in
for the board and its card.

## Coming from play_scrub.h

If you copied the old `play_scrub.h` and `play_scrub.cpp` into your Audio library folder, remove
them and the line you added to `Audio.h`, and include `<Scrubby.h>` in your sketch. The calls are
the same. Mode 3 is `STOPPED`, `grabBuffer` is gone (the scrubber reads the card itself), and a
scrubber never given a speed plays forwards, where it used to play backwards.

## Credits and license

Scrubby is MIT (`LICENSE`). The example's recording is the Teensy Audio Library's Recorder
example's, which is in the public domain.
