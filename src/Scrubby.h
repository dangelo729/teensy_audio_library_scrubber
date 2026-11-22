// scrubby — Scrubby.h
// the umbrella. everywhere it brings in sb::Playhead, the scrubber itself,
// which compiles with nothing but the c++ standard library. on a teensy it
// brings in AudioPlayScrub too, the audio object that plays a file on the
// sd card through it; on a computer that header is empty.
#pragma once

#include "core/sb_playhead.h"

#include "AudioPlayScrub.h"
