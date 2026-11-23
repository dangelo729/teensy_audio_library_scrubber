# Changelog

All notable changes to Scrubby are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-11-23

The first release as a library, in place of the `play_scrub.h` and `play_scrub.cpp` once
copied into the Audio library's folder.

- The calls are the same. Mode 3 is `STOPPED`, `grabBuffer` is gone (the scrubber reads the
  card itself), and a scrubber never given a speed plays forwards, where it used to play
  backwards.
- The playhead (`sb::Playhead`) is plain C++ and is tested on a computer.
