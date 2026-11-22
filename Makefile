# scrubby — Makefile
# a doorway: the host rig lives in extras/host, and this forwards to it so
# `make test` works from the repository root. (the arduino build ignores
# this file.)

.PHONY: all test clean

all test clean:
	$(MAKE) -C extras/host $@
