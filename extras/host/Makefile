# scrubby — extras/host/Makefile
# the host rig: everything compiles here with clang and nothing else.
#
#   make test     build and run every test in tests/
#   make clean
#
# the playhead (src/core) is plain c++. the teensy object
# (src/AudioPlayScrub.cpp) is built here too, against stub/, the board's
# stand-ins: a card whose files are rules, and a pool of audio blocks that
# counts what is out of it. the tests run with clang's address and
# undefined-behaviour checks on, so a write past the window is caught.
#
# gnu make 3.81 compatible (the macos default).

CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
            -DARDUINO -Istub -I../../src -I.

BUILD := build

WRAP_SRC := $(wildcard ../../src/*.cpp)
TEST_SRC := $(wildcard tests/*.cpp)

WRAP_OBJ := $(patsubst ../../src/%.cpp,$(BUILD)/wrap_%.o,$(WRAP_SRC))
TEST_OBJ := $(patsubst tests/%.cpp,$(BUILD)/test_%.o,$(TEST_SRC))

.PHONY: all test clean

all: $(BUILD)/run_tests

test: $(BUILD)/run_tests
	./$(BUILD)/run_tests

clean:
	rm -rf $(BUILD)

$(BUILD)/run_tests: $(TEST_OBJ) $(WRAP_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

# -MMD -MP: header dependencies in build/*.d, included below
$(BUILD)/wrap_%.o: ../../src/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/test_%.o: tests/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(wildcard $(BUILD)/*.d)
