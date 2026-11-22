// scrubby — extras/host/stub/Arduino.h
// the board's stand-in on a computer: just what AudioPlayScrub asks of
// <Arduino.h>. there are no interrupts here to mask.
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

inline void __disable_irq() {}
inline void __enable_irq() {}
