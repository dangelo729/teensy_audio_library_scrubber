// scrubby — tests/main.cpp
// runs every registered test, or those whose name contains argv[1].
#include <cstring>

#include "sb_test.h"

namespace sbtest {
Case*& head() { static Case* h = nullptr; return h; }
int& failures() { static int f = 0; return f; }
int& checks() { static int c = 0; return c; }
}  // namespace sbtest

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0, failedCases = 0;
    for (sbtest::Case* c = sbtest::head(); c; c = c->next) {
        if (filter && !std::strstr(c->name, filter)) continue;
        const int before = sbtest::failures();
        printf("  %s\n", c->name);
        c->fn();
        ++ran;
        if (sbtest::failures() != before) ++failedCases;
    }
    printf("%d tests, %d checks, %d failures\n", ran, sbtest::checks(), sbtest::failures());
    return failedCases ? 1 : 0;
}
