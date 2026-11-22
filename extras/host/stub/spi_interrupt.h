// scrubby — extras/host/stub/spi_interrupt.h
// the audio library's count of who reads the card from the audio
// interrupt, kept here for the tests to read.
#pragma once

namespace host {
inline int guards = 0;
}  // namespace host

inline void AudioStartUsingSPI(void) { ++host::guards; }
inline void AudioStopUsingSPI(void) { --host::guards; }
